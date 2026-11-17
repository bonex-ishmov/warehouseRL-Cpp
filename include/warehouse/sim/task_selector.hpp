#pragma once

#include "warehouse/io/config.hpp"
#include "warehouse/sim/task_manager.hpp"
#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

enum class Phase : uint8_t { ToPickup = 0, ToDropoff = 1, ToCharger = 2, Idle = 3 };
const char* toString(Phase p);

struct Decision {
    Phase phase = Phase::Idle;
    int taskId = -1;
    Vec2 goal;
};

/// Nivel alto de la jerarquía: decide qué hacer (tarea, recarga o esperar).
/// Puntúa las tareas por prioridad, urgencia y distancia, y reserva energía
/// suficiente para llegar a una estación después de entregar.
class TaskSelector {
public:
    struct Context {
        Vec2 robotPos;
        double now = 0.0;
        float batteryWh = 0.0f;
        float totalMass = 50.0f;
        int carryingTask = -1;
    };

    void configure(const ScenarioConfig& cfg) { cfg_ = &cfg; }

    Decision decide(const Context& ctx, const TaskManager& tasks, const WarehouseMap& map) const;
    /// Energía estimada (Wh) para recorrer una distancia con una masa dada.
    float energyForDistance(float meters, float mass) const;
    Vec2 nearestCharger(Vec2 p, const WarehouseMap& map) const;
    float score(const Task& t, const Context& ctx) const;

private:
    const ScenarioConfig* cfg_ = nullptr;
};

}  // namespace wrl
