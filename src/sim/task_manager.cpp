#include "warehouse/sim/task_manager.hpp"

#include <numeric>

namespace wrl {

const char* toString(Priority p) {
    switch (p) {
        case Priority::Low: return "baja";
        case Priority::Medium: return "media";
        case Priority::High: return "alta";
        case Priority::Critical: return "critica";
    }
    return "?";
}

const char* toString(TaskStatus s) {
    switch (s) {
        case TaskStatus::Scheduled: return "programada";
        case TaskStatus::Pending: return "pendiente";
        case TaskStatus::Assigned: return "asignada";
        case TaskStatus::Carrying: return "en_transporte";
        case TaskStatus::Delivered: return "entregada";
    }
    return "?";
}

void TaskManager::generate(const ScenarioConfig& cfg, const WarehouseMap& map, AStarPlanner& planner, Rng& rng) {
    tasks_.clear();
    const auto& tp = cfg.tasks;

    std::vector<int> allowedLoads;
    for (int i = 0; i < static_cast<int>(cfg.loadTypes.size()); ++i) {
        const auto& name = cfg.loadTypes[static_cast<size_t>(i)].name;
        if (tp.loadTypes.empty() || std::find(tp.loadTypes.begin(), tp.loadTypes.end(), name) != tp.loadTypes.end())
            allowedLoads.push_back(i);
    }
    const float wsum = std::accumulate(tp.priorityWeights.begin(), tp.priorityWeights.end(), 0.0f);
    auto samplePriority = [&] {
        float u = rng.uniform(0.0f, wsum);
        for (int i = 0; i < 4; ++i) {
            u -= tp.priorityWeights[static_cast<size_t>(i)];
            if (u <= 0.0f) return static_cast<Priority>(i);
        }
        return Priority::Low;
    };

    // Tiempo medio de una tarea (para escalonar plazos del lote inicial).
    const float diag = std::sqrt(map.width * map.width + map.height * map.height);
    double arrival = 0.0;
    double queueOffset = 0.0;
    for (int i = 0; i < tp.count; ++i) {
        Task t;
        t.id = i;
        t.pickup = map.pickups[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(map.pickups.size()) - 1))];
        t.destination = map.dropoffs[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(map.dropoffs.size()) - 1))];
        t.priority = samplePriority();
        t.loadType = allowedLoads[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(allowedLoads.size()) - 1))];
        t.routeLength = planner.shortestLength(t.pickup, t.destination).value_or(distance(t.pickup, t.destination) * 1.5f);

        if (i < tp.initial) {
            t.releaseTime = 0.0;
        } else {
            // Llegadas con intervalo exponencial.
            arrival += -std::log(std::max(1e-6f, rng.uniform())) * tp.arrivalInterval;
            t.releaseTime = arrival;
        }
        if (tp.deadlines) {
            // Estimación: llegar a la recogida (media diagonal) más el transporte.
            const double est = (0.5 * diag + t.routeLength) / cfg.sim.nominalSpeed + 2.0 * cfg.sim.dockTime;
            double slack = rng.uniform(tp.slackMin, tp.slackMax);
            if (t.priority == Priority::Critical) slack *= 0.8;
            double extra = 0.0;
            if (i < tp.initial) {
                extra = queueOffset;  // las del lote inicial esperan a las anteriores
                queueOffset += est;
            }
            t.deadline = t.releaseTime + tp.deadlineBase + slack * est + extra;
        }
        tasks_.push_back(t);
    }
}

int TaskManager::release(double now) {
    int critical = 0;
    for (auto& t : tasks_) {
        if (t.status == TaskStatus::Scheduled && t.releaseTime <= now) {
            t.status = TaskStatus::Pending;
            if (t.priority == Priority::Critical) ++critical;
        }
    }
    return critical;
}

int TaskManager::collectNewCriticalOverdue(double now) {
    int n = 0;
    for (auto& t : tasks_) {
        if (t.priority == Priority::Critical && !t.overdueReported && t.status != TaskStatus::Delivered &&
            t.status != TaskStatus::Scheduled && now > t.deadline) {
            t.overdueReported = true;
            ++n;
        }
    }
    return n;
}

void TaskManager::assign(int id) {
    for (auto& t : tasks_)
        if (t.status == TaskStatus::Assigned) t.status = TaskStatus::Pending;
    get(id).status = TaskStatus::Assigned;
}

void TaskManager::unassign(int id) {
    if (get(id).status == TaskStatus::Assigned) get(id).status = TaskStatus::Pending;
}

void TaskManager::markPicked(int id, double now) {
    Task& t = get(id);
    t.status = TaskStatus::Carrying;
    t.pickedAt = now;
}

bool TaskManager::markDelivered(int id, double now) {
    Task& t = get(id);
    t.status = TaskStatus::Delivered;
    t.deliveredAt = now;
    return now <= t.deadline;
}

void TaskManager::markDropped(int id) {
    Task& t = get(id);
    t.status = TaskStatus::Pending;
    t.pickedAt = -1.0;
    ++t.incidents;
}

bool TaskManager::allDelivered() const {
    return std::all_of(tasks_.begin(), tasks_.end(), [](const Task& t) { return t.status == TaskStatus::Delivered; });
}

int TaskManager::countWithStatus(TaskStatus s) const {
    return static_cast<int>(std::count_if(tasks_.begin(), tasks_.end(), [s](const Task& t) { return t.status == s; }));
}

int TaskManager::openCount() const {
    return static_cast<int>(std::count_if(tasks_.begin(), tasks_.end(), [](const Task& t) { return t.open(); }));
}

}  // namespace wrl
