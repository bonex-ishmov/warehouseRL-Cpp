#pragma once

#include <vector>

#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/physics/dynamic_obstacles.hpp"
#include "warehouse/physics/physics_world.hpp"

namespace wrl {

/// Obstáculo dinámico tal como lo percibe el robot (marco local del robot).
struct PerceivedObstacle {
    Vec2 relPos;   ///< posición relativa en el marco del robot
    Vec2 relVel;   ///< velocidad relativa en el marco del robot
    Vec2 worldPos;
    Vec2 worldVel;
    float radius = 0.3f;
    bool worker = true;
    float gap = 0.0f;       ///< distancia entre la carrocería del robot y la superficie del obstáculo, m
    Vec2 normal{1, 0};      ///< dirección (mundo) del punto más cercano del robot al del obstáculo
    float closing = 0.0f;   ///< velocidad relativa de acercamiento a lo largo de normal, m/s (> 0: se acercan)
};

/// LiDAR 2D por lanzamiento de rayos y odometría con ruido.
class SensorSuite {
public:
    void configure(const SensorParams& params);
    const SensorParams& params() const { return params_; }

    /// Distancias medidas desde el centro del robot, una por rayo, en metros.
    void scanLidar(const PhysicsWorld& world, Pose robot, Rng& rng, std::vector<float>& out) const;
    /// Los k obstáculos dinámicos más cercanos (por distancia entre superficies) dentro
    /// del alcance y con línea de visión. robotHalf: medio largo y medio ancho del robot.
    void perceiveObstacles(const PhysicsWorld& world, Pose robot, Vec2 robotVel, Vec2 robotHalf,
                           const std::vector<DynamicObstacle>& obstacles, std::vector<PerceivedObstacle>& out) const;
    float noisySpeed(float trueValue, Rng& rng) const;

    float rayAngle(int i) const;

private:
    SensorParams params_;
};

}  // namespace wrl
