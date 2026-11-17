#include "warehouse/sensors/sensor_suite.hpp"

namespace wrl {

void SensorSuite::configure(const SensorParams& params) { params_ = params; }

float SensorSuite::rayAngle(int i) const {
    return -kPi + (2.0f * kPi) * static_cast<float>(i) / static_cast<float>(params_.lidarRays);
}

void SensorSuite::scanLidar(const PhysicsWorld& world, Pose robot, Rng& rng, std::vector<float>& out) const {
    out.resize(static_cast<size_t>(params_.lidarRays));
    const float range = params_.lidarRange;
    for (int i = 0; i < params_.lidarRays; ++i) {
        const Vec2 dir = fromAngle(robot.theta + rayAngle(i));
        float d = world.castRay(robot.pos, dir, range, kCatStatic | kCatObstacle);
        if (params_.lidarNoise > 0.0f && d < range) d += rng.normal(0.0f, params_.lidarNoise);
        if (params_.lidarDropout > 0.0f && rng.bernoulli(params_.lidarDropout)) d = range;
        out[static_cast<size_t>(i)] = std::clamp(d, 0.0f, range);
    }
}

void SensorSuite::perceiveObstacles(const PhysicsWorld& world, Pose robot, Vec2 robotVel,
                                    const std::vector<DynamicObstacle>& obstacles,
                                    std::vector<PerceivedObstacle>& out) const {
    out.clear();
    for (const auto& o : obstacles) {
        const Vec2 d = o.pos - robot.pos;
        const float dist = d.length();
        if (dist > params_.lidarRange) continue;
        // Línea de visión: las estanterías tapan lo que hay detrás.
        if (dist > 0.5f && world.castRay(robot.pos, d / dist, dist - 0.3f, kCatStatic) < dist - 0.35f) continue;
        PerceivedObstacle p;
        p.worldPos = o.pos;
        p.worldVel = o.vel;
        p.relPos = toLocal(d, robot.theta);
        p.relVel = toLocal(o.vel - robotVel, robot.theta);
        p.radius = o.isWorker() ? o.halfExtents.x : o.halfExtents.length();
        p.worker = o.isWorker();
        out.push_back(p);
    }
    std::sort(out.begin(), out.end(),
              [](const PerceivedObstacle& a, const PerceivedObstacle& b) { return a.relPos.lengthSq() < b.relPos.lengthSq(); });
    if (out.size() > static_cast<size_t>(params_.nearestObstacles)) out.resize(static_cast<size_t>(params_.nearestObstacles));
}

float SensorSuite::noisySpeed(float trueValue, Rng& rng) const {
    if (params_.odometryNoise <= 0.0f) return trueValue;
    return trueValue * (1.0f + rng.normal(0.0f, params_.odometryNoise));
}

}  // namespace wrl
