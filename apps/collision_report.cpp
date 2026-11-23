// Analiza las circunstancias de cada choque: contra qué fue, quién se movía
// hacia quién, desde qué lado llegó y qué percibía el robot de ese obstáculo
// un segundo antes (rayos del LiDAR que lo tocaban, si estaba entre los
// obstáculos de la observación o si lo tapaba una estantería).
//
//   wrl_collisions --policy sac --model models/sac_final.pt --scenario configs/scenarios/e5_combinado.json
//                  [--episodes 100] [--csv choques.csv]

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "cli.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/env/warehouse_env.hpp"
#ifdef WRL_HAS_TORCH
#include "warehouse/agent/sac_policy.hpp"
#endif

using namespace wrl;
namespace fs = std::filesystem;

namespace {

/// Lo que el robot sabía de un obstáculo en un paso de control.
struct ObsView {
    float gap = 1e9f;     ///< hueco aproximado entre las dos superficies (m)
    int rays = 0;         ///< rayos del LiDAR que lo tocan
    bool inObs = false;   ///< está entre los obstáculos de la observación
    bool los = false;     ///< en rango y sin estantería en medio
};

struct Snapshot {
    Pose pose;
    Vec2 vel;
    float v = 0.0f;
    Action action;
    std::vector<Vec2> obsPos, obsVel;
    std::vector<ObsView> views;
    int losCount = 0;     ///< obstáculos en rango y a la vista (antes de quedarse con los N más cercanos)
    float minLidar = 0.0f;
    float w = 0.0f;
};

/// Una caída de carga: qué aceleración la tiró y qué hacía el robot.
struct LoadDrop {
    int type = 0;
    float ax = 0.0f, ay = 0.0f;       ///< aceleración filtrada en el marco del robot, m/s²
    float limX = 0.0f, limY = 0.0f;   ///< límites de ese tipo de carga
    float v = 0.0f, w = 0.0f;         ///< antes del paso
    float actionV = 0.0f, actionW = 0.0f;
    bool collided = false;            ///< hubo un choque en el mismo paso
    bool shield = false;              ///< la capa de seguridad recortó la consigna
    float vAfter = 0.0f, wAfter = 0.0f, slip = 0.0f;  ///< después del paso; slip = velocidad lateral
};

struct Collision {
    std::string scenario;
    uint64_t seed = 0;
    double time = 0.0;
    int severity = 0;     ///< 0 roce, 1 leve, 2 grave
    std::string target;   ///< operario, montacargas, estatico
    float robotV = 0.0f;
    float robotApproach = 0.0f;  ///< velocidad del robot hacia el obstáculo
    float obsApproach = 0.0f;    ///< velocidad del obstáculo hacia el robot
    float bearingDeg = 0.0f;     ///< dirección del obstáculo en el marco del robot (0 = delante)
    float action = 0.0f;
    bool carrying = false;
    // Un segundo antes (o lo más atrás que haya):
    float gap1s = 0.0f;
    int rays1s = 0, raysNow = 0;
    bool inObs1s = false, inObsNow = false, los1s = false;
    int losCount1s = 0;
    float minLidarNow = 0.0f;    ///< lectura mínima del LiDAR antes del choque (m)
    // Contexto (sobre todo para choques contra lo estático):
    float minLidar1s = 0.0f, v1s = 0.0f;
    float omega = 0.0f;          ///< giro del robot antes del choque, rad/s
    float friction = 0.0f;       ///< adherencia bajo el robot
    float goalDist = 0.0f;       ///< distancia al objetivo actual, m
};

float robotSupport(const RobotParams& rp, Vec2 dirLocal) {
    return rp.halfLength * std::abs(dirLocal.x) + rp.halfWidth * std::abs(dirLocal.y);
}

/// Distancia del centro del robot a la superficie del obstáculo.
float surfaceDist(const DynamicObstacle& o, Vec2 p) {
    if (o.isWorker()) return (p - o.pos).length() - o.halfExtents.x;
    const Vec2 l = toLocal(p - o.pos, o.theta);
    const Vec2 q{std::max(std::abs(l.x) - o.halfExtents.x, 0.0f), std::max(std::abs(l.y) - o.halfExtents.y, 0.0f)};
    return q.length();
}

Snapshot capture(const WarehouseEnv& env) {
    Snapshot s;
    const auto& rp = env.config().robot;
    const auto& sp = env.config().sensors;
    const auto& world = env.physics();
    s.pose = env.robot().pose();
    s.vel = env.robot().velocity();
    s.v = env.robot().forwardSpeed();
    s.action = env.lastAction();
    s.minLidar = *std::min_element(env.lidar().begin(), env.lidar().end());
    s.w = env.robot().angularSpeed();
    const auto& obs = env.obstacles();
    s.views.resize(obs.size());

    std::vector<int> rayHits(obs.size(), 0);
    for (int i = 0; i < sp.lidarRays; ++i) {
        const Vec2 dir = fromAngle(s.pose.theta + env.sensors().rayAngle(i));
        ShapeTag hit{BodyTag::Wall, -1};
        const float d = world.castRay(s.pose.pos, dir, sp.lidarRange, kCatStatic | kCatObstacle, &hit);
        if (d < sp.lidarRange && (hit.tag == BodyTag::Worker || hit.tag == BodyTag::Forklift) && hit.index >= 0 &&
            hit.index < static_cast<int>(obs.size()))
            ++rayHits[static_cast<size_t>(hit.index)];
    }
    for (size_t k = 0; k < obs.size(); ++k) {
        const auto& o = obs[k];
        s.obsPos.push_back(o.pos);
        s.obsVel.push_back(o.vel);
        ObsView& v = s.views[k];
        const Vec2 d = o.pos - s.pose.pos;
        const float dist = d.length();
        const Vec2 u = dist > 1e-4f ? d / dist : Vec2{1, 0};
        v.gap = surfaceDist(o, s.pose.pos) - robotSupport(rp, toLocal(u, s.pose.theta));
        v.rays = rayHits[k];
        v.los = dist <= sp.lidarRange &&
                !(dist > 0.5f && world.castRay(s.pose.pos, u, dist - 0.3f, kCatStatic) < dist - 0.35f);
        if (v.los) ++s.losCount;
        for (const auto& p : env.perceived())
            if ((p.worldPos - o.pos).lengthSq() < 1e-6f) v.inObs = true;
    }
    return s;
}

void printDrops(const ScenarioConfig& cfg, const std::vector<LoadDrop>& ds) {
    if (ds.empty()) return;
    const int n = static_cast<int>(ds.size());
    int lateral = 0, longi = 0, both = 0, collided = 0, shield = 0, braking = 0;
    double sumVW = 0, sumV = 0, sumW = 0;
    std::vector<int> byType(cfg.loadTypes.size(), 0);
    for (const auto& d : ds) {
        const bool la = std::abs(d.ay) > d.limY, lo = std::abs(d.ax) > d.limX;
        if (la && lo) ++both;
        else if (la) ++lateral;
        else ++longi;
        if (d.collided) ++collided;
        if (d.shield) ++shield;
        if (lo && d.ax < 0 && d.v > 0) ++braking;
        sumVW += std::abs(d.v * d.w), sumV += std::abs(d.v), sumW += std::abs(d.w);
        ++byType[static_cast<size_t>(d.type)];
    }
    auto pct = [n](int a) { return 100.0 * a / n; };
    std::printf("\n  caídas de carga %d: lateral %.0f%%, longitudinal %.0f%% (frenando %.0f%%), ambas %.0f%% | "
                "con choque en el mismo paso %.0f%% | capa activa %.0f%%\n",
                n, pct(lateral), pct(longi), pct(braking), pct(both), pct(collided), pct(shield));
    // Sin choque: ¿la curva (v·giro) ya superaba el límite lateral, o fue un derrape?
    int curve = 0, noColl = 0;
    double slip = 0, vwAfter = 0;
    for (const auto& d : ds) {
        if (d.collided) continue;
        ++noColl;
        vwAfter += std::abs(d.vAfter * d.wAfter);
        slip += std::abs(d.slip);
        if (std::abs(d.vAfter * d.wAfter) > 0.8f * d.limY) ++curve;
    }
    if (noColl)
        std::printf("    sin choque (%d): |v·giro| después %.2f m/s², > 80%% del límite lateral %.0f%%, |vel. lateral| %.2f m/s\n",
                    noColl, vwAfter / noColl, 100.0 * curve / noColl, slip / noColl);
    std::printf("    antes: |v| %.2f m/s, |giro| %.2f rad/s, |v·giro| %.2f m/s² | por tipo:", sumV / n, sumW / n, sumVW / n);
    for (size_t t = 0; t < byType.size(); ++t)
        if (byType[t]) std::printf(" %s %d", cfg.loadTypes[t].name.c_str(), byType[t]);
    std::printf("\n");
}

std::unique_ptr<Policy> createPolicy(const std::string& name, const std::string& model) {
#ifdef WRL_HAS_TORCH
    if (name == "sac") return makeSacPolicy(model, "cpu");
#endif
    return makeBuiltinPolicy(name);
}

void printSummary(const std::vector<Collision>& cs, int episodes) {
    auto pct = [](int a, int b) { return b > 0 ? 100.0 * a / b : 0.0; };
    const char* sevName[3] = {"roces", "leves", "graves"};
    for (const char* target : {"operario", "montacargas", "estatico"}) {
        std::vector<const Collision*> sel;
        for (const auto& c : cs)
            if (c.target == target) sel.push_back(&c);
        int bySev[3] = {0, 0, 0};
        for (auto* c : sel) ++bySev[c->severity];
        std::printf("\n  %-11s %4zu choques (%.2f/episodio): %d roces, %d leves, %d graves\n", target, sel.size(),
                    static_cast<double>(sel.size()) / episodes, bySev[0], bySev[1], bySev[2]);
        for (int sev = 1; sev <= 2; ++sev) {
            std::vector<const Collision*> s2;
            for (auto* c : sel)
                if (c->severity == sev) s2.push_back(c);
            if (s2.empty()) continue;
            const int n = static_cast<int>(s2.size());
            int front = 0, side = 0, rear = 0, robotFault = 0, obsFault = 0, both = 0, reversing = 0, braking = 0, stopped = 0;
            int noRays1s = 0, notInObs1s = 0, occluded1s = 0, crowded1s = 0, notInObsNow = 0, noRaysNow = 0, carrying = 0;
            double sumV = 0, sumRa = 0, sumOa = 0, sumGap = 0;
            for (auto* c : s2) {
                const float b = std::abs(c->bearingDeg);
                (b < 45 ? front : b > 135 ? rear : side)++;
                const bool r = c->robotApproach > 0.2f, o = c->obsApproach > 0.2f;
                if (r && o) ++both;
                else if (r) ++robotFault;
                else if (o) ++obsFault;
                if (c->robotV < -0.05f) ++reversing;
                else if (c->robotV < 0.1f) ++stopped;
                else if (c->action < -0.05f) ++braking;  // avanzando y pidiendo marcha atrás = frenada a fondo
                if (c->target != std::string("estatico")) {
                    if (c->rays1s == 0) ++noRays1s;
                    if (c->raysNow == 0) ++noRaysNow;
                    if (!c->inObs1s) ++notInObs1s;
                    if (!c->inObsNow) ++notInObsNow;
                    if (!c->los1s) ++occluded1s;
                    if (c->los1s && !c->inObs1s) ++crowded1s;
                }
                if (c->carrying) ++carrying;
                sumV += c->robotV;
                sumRa += c->robotApproach;
                sumOa += c->obsApproach;
                sumGap += c->gap1s;
            }
            std::printf("    %s (%d):\n", sevName[sev], n);
            std::printf("      lado: delante %.0f%%, lateral %.0f%%, detrás %.0f%% | robot parado %.0f%%, frenando %.0f%%, marcha atrás %.0f%% | con carga %.0f%%\n",
                        pct(front, n), pct(side, n), pct(rear, n), pct(stopped, n), pct(braking, n), pct(reversing, n),
                        pct(carrying, n));
            std::printf("      quién se acercó: robot %.0f%%, obstáculo %.0f%%, ambos %.0f%%, ninguno >0.2 m/s %.0f%%\n",
                        pct(robotFault, n), pct(obsFault, n), pct(both, n), pct(n - robotFault - obsFault - both, n));
            std::printf("      medias: v robot %.2f m/s, robot→obst %.2f m/s, obst→robot %.2f m/s, hueco 1 s antes %.2f m\n",
                        sumV / n, sumRa / n, sumOa / n, sumGap / n);
            if (std::string(target) != "estatico")
                std::printf("      percepción 1 s antes: sin rayos del LiDAR %.0f%%, fuera de la observación %.0f%% "
                            "(tapado %.0f%%, desplazado por otros más cercanos %.0f%%)\n"
                            "      justo antes del choque: sin rayos %.0f%%, fuera de la observación %.0f%%\n",
                            pct(noRays1s, n), pct(notInObs1s, n), pct(occluded1s, n), pct(crowded1s, n), pct(noRaysNow, n),
                            pct(notInObsNow, n));
            else {
                // ¿Lo veía venir? LiDAR (desde el centro) y velocidad 1 s antes, giro, suelo y objetivo.
                double l1 = 0, v1 = 0, om = 0, mu = 0, gd = 0;
                int slippery = 0, nearGoal = 0, turning = 0;
                for (auto* c : s2) {
                    l1 += c->minLidar1s, v1 += c->v1s, om += std::abs(c->omega), mu += c->friction, gd += c->goalDist;
                    if (c->friction < 0.7f) ++slippery;
                    if (c->goalDist < 1.5f) ++nearGoal;
                    if (std::abs(c->omega) > 0.5f) ++turning;
                }
                std::printf("      1 s antes: LiDAR mín %.2f m, v %.2f m/s | giro medio %.2f rad/s (girando >0.5: %.0f%%) | "
                            "adherencia media %.2f (resbaladizo %.0f%%) | objetivo a %.1f m (a <1.5 m: %.0f%%)\n",
                            l1 / n, v1 / n, om / n, pct(turning, n), mu / n, pct(slippery, n), gd / n, pct(nearGoal, n));
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("help") || !args.has("scenario")) {
        std::puts("Uso: wrl_collisions --scenario e.json [--scenario ...] [--policy sac|dwa] [--model m.pt]\n"
                  "                     [--episodes 100] [--seeds 5] [--threads N] [--csv choques.csv]\n"
                  "                     [--shield on|off]");
        return args.has("help") ? 0 : 1;
    }
    const std::string policyName = args.get("policy", "sac");
    const std::string model = args.get("model", "models/sac_final.pt");
    const int episodes = args.getInt("episodes", 100);
    const int seeds = std::max(1, args.getInt("seeds", 5));
    const int threads = std::max(1, args.getInt("threads", static_cast<int>(std::thread::hardware_concurrency()) - 1));

    std::vector<Collision> all;
    for (const auto& scenarioPath : args.all("scenario")) {
        ScenarioConfig cfg = loadScenario(scenarioPath);
        if (args.has("shield")) cfg.shield.enabled = args.get("shield") != "off";
        const int perSeed = std::max(1, episodes / seeds);
        std::vector<uint64_t> jobs;
        for (int s = 0; s < seeds; ++s)
            for (int e = 0; e < perSeed; ++e)
                jobs.push_back(mixSeed(0xE7A1'0000ull + static_cast<uint64_t>(s) * 7919ull, static_cast<uint64_t>(e)));

        std::vector<Collision> found;
        std::vector<LoadDrop> drops;
        std::mutex mu;
        std::atomic<size_t> next{0};
        auto worker = [&] {
            WarehouseEnv env(cfg);
            auto policy = createPolicy(policyName, model);
            while (true) {
                const size_t j = next++;
                if (j >= jobs.size()) return;
                env.reset(jobs[j]);
                policy->reset(jobs[j]);
                std::deque<Snapshot> hist;
                hist.push_back(capture(env));
                std::vector<Collision> local;
                std::vector<LoadDrop> localDrops;
                while (true) {
                    const EpisodeMetrics before = env.metrics();
                    const bool carrying = env.carryingTask() >= 0;
                    const StepResult r = env.step(policy->act(env));
                    const EpisodeMetrics& after = env.metrics();
                    if (r.events.loadDrops > 0 && r.events.loadDropType >= 0) {
                        const auto& lt = cfg.loadTypes[static_cast<size_t>(r.events.loadDropType)];
                        const auto& e = r.events;
                        localDrops.push_back({e.loadDropType, e.loadDropAccel.x, e.loadDropAccel.y, lt.longitudinalLimit,
                                              lt.lateralLimit, hist.back().v, hist.back().w, env.lastAction().linear,
                                              env.lastAction().angular,
                                              e.collisions[0] + e.collisions[1] + e.collisions[2] > 0, env.shieldActive(),
                                              env.robot().forwardSpeed(), env.robot().angularSpeed(),
                                              env.robot().lateralSpeed()});
                    }
                    for (int sev = 0; sev < 3; ++sev) {
                        for (int k = 0; k < after.collisions[sev] - before.collisions[sev]; ++k) {
                            const Snapshot& now = hist.back();
                            const Snapshot& ago = hist.front();
                            // ¿Contra qué? El obstáculo con menor hueco tras el paso; si ninguno está pegado, estático.
                            const Pose pose = env.robot().pose();
                            const auto& obs = env.obstacles();
                            int best = -1;
                            float bestGap = 0.25f;
                            for (size_t q = 0; q < obs.size(); ++q) {
                                const Vec2 d = obs[q].pos - pose.pos;
                                const float dist = d.length();
                                const Vec2 u = dist > 1e-4f ? d / dist : Vec2{1, 0};
                                const float gap = surfaceDist(obs[q], pose.pos) - robotSupport(env.config().robot, toLocal(u, pose.theta));
                                if (gap < bestGap) bestGap = gap, best = static_cast<int>(q);
                            }
                            Collision c;
                            c.scenario = cfg.name;
                            c.seed = jobs[j];
                            c.time = env.time();
                            c.severity = sev;
                            c.robotV = now.v;
                            c.action = env.lastAction().linear;
                            c.carrying = carrying;
                            c.minLidarNow = now.minLidar;
                            c.minLidar1s = ago.minLidar;
                            c.v1s = ago.v;
                            c.omega = env.robot().angularSpeed();
                            c.friction = env.frictionUnderRobot();
                            c.goalDist = distance(pose.pos, env.decision().goal);
                            if (best >= 0) {
                                const auto b = static_cast<size_t>(best);
                                c.target = obs[b].isWorker() ? "operario" : "montacargas";
                                const Vec2 d = now.obsPos[b] - now.pose.pos;
                                const Vec2 u = normalized(d);
                                c.robotApproach = dot(now.vel, u);
                                c.obsApproach = dot(now.obsVel[b], u * -1.0f);
                                const Vec2 l = toLocal(d, now.pose.theta);
                                c.bearingDeg = std::atan2(l.y, l.x) * 180.0f / 3.14159265f;
                                c.gap1s = ago.views[b].gap;
                                c.rays1s = ago.views[b].rays;
                                c.raysNow = now.views[b].rays;
                                c.inObs1s = ago.views[b].inObs;
                                c.inObsNow = now.views[b].inObs;
                                c.los1s = ago.views[b].los;
                                c.losCount1s = ago.losCount;
                            } else {
                                c.target = "estatico";
                                // Dirección del obstáculo: el rayo más corto antes del choque.
                                const auto& lid = env.lidar();
                                const auto it = std::min_element(lid.begin(), lid.end());
                                const float a = env.sensors().rayAngle(static_cast<int>(it - lid.begin()));
                                c.bearingDeg = a * 180.0f / 3.14159265f;
                                const Vec2 u = fromAngle(now.pose.theta + a);
                                c.robotApproach = dot(now.vel, u);
                            }
                            local.push_back(c);
                        }
                    }
                    hist.push_back(capture(env));
                    if (hist.size() > 11) hist.pop_front();  // 1 s a 10 Hz
                    if (r.done()) break;
                }
                std::lock_guard lock(mu);
                found.insert(found.end(), local.begin(), local.end());
                drops.insert(drops.end(), localDrops.begin(), localDrops.end());
            }
        };
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t) pool.emplace_back(worker);
        for (auto& t : pool) t.join();

        std::printf("\n== %s (%s, %zu episodios)", cfg.name.c_str(), policyName.c_str(), jobs.size());
        printSummary(found, static_cast<int>(jobs.size()));
        printDrops(cfg, drops);
        all.insert(all.end(), found.begin(), found.end());
    }

    if (args.has("csv")) {
        std::ofstream f(args.get("csv"));
        f << "escenario,semilla,t,gravedad,contra,v_robot,robot_hacia,obst_hacia,angulo,accion,con_carga,"
             "hueco_1s,rayos_1s,rayos_ahora,en_obs_1s,en_obs_ahora,a_la_vista_1s,visibles_1s,lidar_min,lidar_min_1s,v_1s,"
             "giro,adherencia,dist_objetivo\n";
        for (const auto& c : all)
            f << c.scenario << ',' << c.seed << ',' << c.time << ',' << c.severity << ',' << c.target << ',' << c.robotV << ','
              << c.robotApproach << ',' << c.obsApproach << ',' << c.bearingDeg << ',' << c.action << ',' << c.carrying << ','
              << c.gap1s << ',' << c.rays1s << ',' << c.raysNow << ',' << c.inObs1s << ',' << c.inObsNow << ',' << c.los1s
              << ',' << c.losCount1s << ',' << c.minLidarNow << ',' << c.minLidar1s << ',' << c.v1s << ',' << c.omega << ','
              << c.friction << ',' << c.goalDist << '\n';
    }
    return 0;
}
