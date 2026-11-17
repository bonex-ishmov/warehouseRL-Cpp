#pragma once

#include <vector>

#include <box2d/box2d.h>

#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/planning/astar.hpp"
#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

struct DynamicObstacle {
    ObstacleSpec::Kind kind = ObstacleSpec::Kind::Worker;
    b2BodyId body{};
    std::vector<Vec2> waypoints;   ///< polilínea que se recorre
    std::vector<uint8_t> stopAt;   ///< 1 si hay pausa al llegar a ese punto
    size_t next = 0;
    float speed = 0.8f;
    float pause = 1.0f;
    float pauseLeft = 0.0f;
    bool stochastic = false;
    bool cautious = false;         ///< se detiene si el robot está delante
    float blockedTime = 0.0f;      ///< tiempo esperando al robot
    Vec2 pos;
    Vec2 vel;
    float theta = 0.0f;
    Vec2 halfExtents;              ///< caja (montacargas) o radio en x (operario)

    bool isWorker() const { return kind == ObstacleSpec::Kind::Worker; }
};

/// Montacargas y operarios como cuerpos cinemáticos: siguen rutas por los
/// pasillos y empujan al robot si chocan (masa infinita a efectos de contacto).
class DynamicObstacleSystem {
public:
    void create(b2WorldId world, const WarehouseMap& map, AStarPlanner& planner, const std::vector<ObstacleSpec>& fixed,
                const ObstacleGenParams& gen, float speedScale, Rng& rng);
    void update(float dt, Vec2 robotPos, AStarPlanner& planner, const WarehouseMap& map, Rng& rng);

    const std::vector<DynamicObstacle>& obstacles() const { return obs_; }

private:
    void addObstacle(b2WorldId world, ObstacleSpec spec, const WarehouseMap& map, AStarPlanner& planner, Rng& rng);
    void extendRoute(DynamicObstacle& o, Vec2 target, AStarPlanner& planner);
    static Vec2 randomTarget(const WarehouseMap& map, Rng& rng);

    std::vector<DynamicObstacle> obs_;
};

}  // namespace wrl
