#pragma once

#include <limits>
#include <vector>

#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/planning/astar.hpp"
#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

enum class Priority : uint8_t { Low = 0, Medium = 1, High = 2, Critical = 3 };
enum class TaskStatus : uint8_t { Scheduled, Pending, Assigned, Carrying, Delivered };

const char* toString(Priority p);
const char* toString(TaskStatus s);

struct Task {
    int id = 0;
    Vec2 pickup;
    Vec2 destination;
    Priority priority = Priority::Low;
    double releaseTime = 0.0;
    double deadline = std::numeric_limits<double>::infinity();
    int loadType = 0;
    TaskStatus status = TaskStatus::Scheduled;
    double pickedAt = -1.0;
    double deliveredAt = -1.0;
    bool overdueReported = false;
    int incidents = 0;
    float routeLength = 0.0f;  ///< recogida → destino por A*

    bool hasDeadline() const { return std::isfinite(deadline); }
    bool open() const { return status == TaskStatus::Pending || status == TaskStatus::Assigned; }
};

/// Cola de tareas: generación, liberación por tiempo, asignación, cierre y plazos.
class TaskManager {
public:
    void generate(const ScenarioConfig& cfg, const WarehouseMap& map, AStarPlanner& planner, Rng& rng);
    void setTasks(std::vector<Task> tasks) { tasks_ = std::move(tasks); }

    /// Libera las tareas cuyo instante llegó. Devuelve cuántas críticas se liberaron.
    int release(double now);
    /// Críticas que vencieron sin entregarse desde la última llamada.
    int collectNewCriticalOverdue(double now);

    void assign(int id);
    void unassign(int id);
    void markPicked(int id, double now);
    /// Devuelve true si se entregó a tiempo.
    bool markDelivered(int id, double now);
    /// La carga se desestabilizó: vuelve a estar pendiente en su punto de recogida.
    void markDropped(int id);

    Task& get(int id) { return tasks_[static_cast<size_t>(id)]; }
    const Task& get(int id) const { return tasks_[static_cast<size_t>(id)]; }
    const std::vector<Task>& tasks() const { return tasks_; }

    bool allDelivered() const;
    int countWithStatus(TaskStatus s) const;
    int openCount() const;

private:
    std::vector<Task> tasks_;
};

}  // namespace wrl
