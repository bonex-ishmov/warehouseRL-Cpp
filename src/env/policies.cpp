#include "warehouse/env/policies.hpp"

#include <stdexcept>

namespace wrl {

Action RandomPolicy::act(const WarehouseEnv&) { return {rng_.uniform(-1.0f, 1.0f), rng_.uniform(-1.0f, 1.0f)}; }

namespace {

// Punto de seguimiento a "ahead" metros por la ruta, salvo que antes haya un
// vértice con giro pronunciado: entonces se apunta primero a ese vértice para
// no intentar cortar la esquina de una estantería.
Vec2 cornerAwareCarrot(const Path& path, Vec2 pos, float ahead) {
    const auto& pts = path.points();
    const float s0 = path.project(pos).s;
    float acc = 0.0f;
    for (size_t i = 1; i + 1 < pts.size(); ++i) {
        acc += distance(pts[i - 1], pts[i]);
        if (acc <= s0 + 0.35f) continue;  // vértice ya alcanzado
        if (acc > s0 + ahead) break;
        const Vec2 d1 = normalized(pts[i] - pts[i - 1]), d2 = normalized(pts[i + 1] - pts[i]);
        if (dot(d1, d2) < std::cos(0.5f)) return pts[i];
    }
    return path.pointAt(s0 + ahead);
}

}  // namespace

Action DwaPolicy::act(const WarehouseEnv& env) {
    const auto& rp = env.config().robot;
    if (!dwa_) dwa_ = std::make_unique<DWAController>(rp);

    DWAController::Input in;
    in.pose = env.robot().pose();
    in.v = env.measuredSpeed();
    in.w = env.measuredAngular();
    // Punto de seguimiento más lejano que el de la observación: el DWA simula
    // 2 s y con uno cercano preferiría ir despacio para no rebasarlo.
    const auto& path = env.path();
    const float ahead = std::max(2.0f, std::abs(env.measuredSpeed()) * 2.0f + 0.5f);
    in.carrot = path.empty() ? env.decision().goal : cornerAwareCarrot(path, env.robot().position(), ahead);
    in.goal = env.decision().goal;
    in.dockRadius = env.config().sim.dockRadius;
    in.totalMass = env.robot().totalMass();
    in.friction = env.frictionUnderRobot();
    in.lidar = &env.lidar();
    in.sensors = &env.sensors();
    in.obstacles = &env.perceived();
    in.path = &path;
    in.dt = env.controlDt();
    if (env.carryingTask() >= 0) {
        const auto& load = env.config().loadTypes[static_cast<size_t>(env.tasks().get(env.carryingTask()).loadType)];
        in.maxAccel = 0.7f * std::min(load.lateralLimit, load.longitudinalLimit);
    }
    const auto cmd = dwa_->compute(in);

    Action a;
    a.linear = cmd.v >= 0 ? cmd.v / rp.maxForwardSpeed : cmd.v / rp.maxReverseSpeed;
    a.angular = cmd.w / rp.maxAngularSpeed;
    return a;
}

std::unique_ptr<Policy> makeBuiltinPolicy(const std::string& name) {
    if (name == "random" || name == "aleatoria") return std::make_unique<RandomPolicy>();
    if (name == "dwa" || name == "astar_dwa") return std::make_unique<DwaPolicy>();
    if (name == "manual") return std::make_unique<ManualPolicy>();
    throw std::runtime_error("política desconocida: " + name + " (use random, dwa, manual o sac)");
}

}  // namespace wrl
