#include "warehouse/physics/dynamic_obstacles.hpp"

#include "warehouse/physics/physics_world.hpp"

namespace wrl {

Vec2 DynamicObstacleSystem::randomTarget(const WarehouseMap& map, Rng& rng) {
    // Mezcla de puntos de interés (donde hay tráfico real) y puntos libres al azar.
    const int choice = rng.uniformInt(0, 3);
    if (choice == 0 && !map.pickups.empty()) return map.pickups[rng.uniformInt(0, static_cast<int>(map.pickups.size()) - 1)];
    if (choice == 1 && !map.dropoffs.empty()) {
        Vec2 d = map.dropoffs[rng.uniformInt(0, static_cast<int>(map.dropoffs.size()) - 1)];
        return d + Vec2{rng.uniform(-1.0f, 1.0f), 0.6f};
    }
    for (int tries = 0; tries < 50; ++tries) {
        const Vec2 p{rng.uniform(1.0f, map.width - 1.0f), rng.uniform(1.0f, map.height - 1.0f)};
        if (!map.insideSolid(p, 0.7f) && !map.inRestricted(p)) return p;
    }
    return map.pickups.empty() ? Vec2{map.width * 0.5f, map.height * 0.5f} : map.pickups.front();
}

void DynamicObstacleSystem::extendRoute(DynamicObstacle& o, Vec2 target, AStarPlanner& planner) {
    const Vec2 from = o.waypoints.empty() ? o.pos : o.waypoints.back();
    const auto path = planner.plan(from, target);
    if (!path) return;
    const auto& pts = path->points();
    for (size_t i = 1; i < pts.size(); ++i) {
        o.waypoints.push_back(pts[i]);
        o.stopAt.push_back(i + 1 == pts.size() ? 1 : 0);
    }
}

void DynamicObstacleSystem::addObstacle(b2WorldId world, ObstacleSpec spec, const WarehouseMap& map,
                                        AStarPlanner& planner, Rng& rng) {
    DynamicObstacle o;
    o.kind = spec.kind;
    o.speed = spec.speed;
    o.pause = spec.pause;
    o.stochastic = spec.stochastic;
    o.cautious = o.isWorker() && rng.bernoulli(0.5f);
    o.halfExtents = o.isWorker() ? Vec2{0.3f, 0.3f} : Vec2{0.6f, 0.4f};

    // Posición inicial alejada del arranque del robot.
    Vec2 start = spec.route.empty() ? randomTarget(map, rng) : spec.route.front();
    for (int tries = 0; tries < 20 && distance(start, map.robotStart.pos) < 4.0f; ++tries) start = randomTarget(map, rng);
    o.pos = start;

    if (spec.route.empty()) {
        for (int i = 0; i < 3; ++i) extendRoute(o, randomTarget(map, rng), planner);
        if (!o.stochastic) extendRoute(o, start, planner);  // circuito cerrado
    } else {
        for (size_t i = 1; i < spec.route.size(); ++i) extendRoute(o, spec.route[i], planner);
        extendRoute(o, spec.route.front(), planner);
    }

    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_kinematicBody;
    bd.position = toB2(o.pos);
    if (!o.waypoints.empty()) {
        const Vec2 d = o.waypoints.front() - o.pos;
        o.theta = std::atan2(d.y, d.x);
    }
    bd.rotation = b2MakeRot(o.theta);
    o.body = b2CreateBody(world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.filter.categoryBits = kCatObstacle;
    sd.filter.maskBits = kCatRobot;  // no chocan con estanterías ni entre sí
    sd.enableContactEvents = true;
    const int index = static_cast<int>(obs_.size());
    if (o.isWorker()) {
        sd.userData = ShapeTag::encode(BodyTag::Worker, index);
        const b2Circle c{{0.0f, 0.0f}, o.halfExtents.x};
        b2CreateCircleShape(o.body, &sd, &c);
    } else {
        sd.userData = ShapeTag::encode(BodyTag::Forklift, index);
        const b2Polygon box = b2MakeBox(o.halfExtents.x, o.halfExtents.y);
        b2CreatePolygonShape(o.body, &sd, &box);
    }
    obs_.push_back(std::move(o));
}

void DynamicObstacleSystem::create(b2WorldId world, const WarehouseMap& map, AStarPlanner& planner,
                                   const std::vector<ObstacleSpec>& fixed, const ObstacleGenParams& gen,
                                   float speedScale, Rng& rng) {
    obs_.clear();
    for (auto spec : fixed) {
        spec.speed *= speedScale;
        addObstacle(world, spec, map, planner, rng);
    }
    const int n = gen.maxCount > 0 ? rng.uniformInt(gen.minCount, gen.maxCount) : 0;
    for (int i = 0; i < n; ++i) {
        ObstacleSpec s;
        s.kind = rng.bernoulli(gen.forkliftFraction) ? ObstacleSpec::Kind::Forklift : ObstacleSpec::Kind::Worker;
        s.speed = rng.uniform(gen.speedMin, gen.speedMax) * speedScale;
        s.pause = rng.uniform(0.5f, 3.0f);
        s.stochastic = rng.bernoulli(gen.stochasticFraction);
        addObstacle(world, s, map, planner, rng);
    }
}

void DynamicObstacleSystem::update(float dt, Vec2 robotPos, AStarPlanner& planner, const WarehouseMap& map, Rng& rng) {
    for (auto& o : obs_) {
        o.pos = fromB2(b2Body_GetPosition(o.body));
        o.theta = b2Rot_GetAngle(b2Body_GetRotation(o.body));
        Vec2 desired{};

        if (o.pauseLeft > 0.0f) {
            o.pauseLeft -= dt;
        } else if (o.next < o.waypoints.size()) {
            const Vec2 target = o.waypoints[o.next];
            const Vec2 d = target - o.pos;
            const float dist = d.length();
            if (dist < std::max(0.05f, o.speed * dt * 1.5f)) {
                if (o.stopAt[o.next]) o.pauseLeft = o.pause;
                ++o.next;
                if (o.next >= o.waypoints.size()) {
                    if (o.stochastic) {
                        const Vec2 last = o.waypoints.back();
                        o.waypoints.assign({last});
                        o.stopAt.assign({0});
                        o.next = 1;
                        extendRoute(o, randomTarget(map, rng), planner);
                    } else {
                        o.next = 0;
                    }
                }
            } else {
                desired = d * (std::min(o.speed, dist / dt) / dist);
            }
        }

        // Todos frenan si el robot está justo delante en su trayectoria; los
        // operarios precavidos además lo hacen con más margen. Nadie cede el
        // paso en cruces laterales, así que evitar choques sigue siendo tarea del robot.
        if (desired.lengthSq() > 0.0f) {
            const Vec2 toRobot = robotPos - o.pos;
            const float reach = (o.cautious ? 1.3f : 0.5f) + o.halfExtents.x;
            if (toRobot.length() < reach && dot(normalized(desired), normalized(toRobot)) > 0.7f) {
                desired = {};
                o.blockedTime += dt;
            } else {
                // Decae despacio: si el robot retrocede un poco y el obstáculo vuelve
                // a frenar, la espera sigue acumulándose y termina por desviarse.
                o.blockedTime = std::max(0.0f, o.blockedTime - 0.25f * dt);
            }
        }
        // Tras esperar un rato, da la vuelta por otro camino (evita bloqueos mutuos
        // en pasillos). El destino nuevo debe alejarse del robot: si la ruta
        // volviera a pasar por donde está, se repetiría el bloqueo indefinidamente.
        if (o.blockedTime > 3.0f) {
            o.blockedTime = 0.0f;
            o.waypoints.assign({o.pos});
            o.stopAt.assign({0});
            o.next = 1;
            o.stochastic = true;
            const Vec2 toRobot = normalized(robotPos - o.pos);
            Vec2 target = randomTarget(map, rng);
            for (int tries = 0; tries < 15; ++tries) {
                const auto path = planner.plan(o.pos, target);
                if (path && path->length() > 1.0f) {
                    const Vec2 dir = normalized(path->pointAt(1.0f) - o.pos);
                    if (dot(dir, toRobot) < 0.2f) break;  // sale en otra dirección
                }
                target = randomTarget(map, rng);
            }
            extendRoute(o, target, planner);
            desired = {};
        }

        o.vel = desired;
        b2Body_SetLinearVelocity(o.body, toB2(desired));
        float w = 0.0f;
        if (desired.lengthSq() > 1e-4f) {
            const float err = wrapAngle(std::atan2(desired.y, desired.x) - o.theta);
            w = std::clamp(err / dt, -3.0f, 3.0f);
        }
        b2Body_SetAngularVelocity(o.body, w);
    }
}

}  // namespace wrl
