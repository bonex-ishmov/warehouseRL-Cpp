#include "warehouse/sim/task_selector.hpp"

namespace wrl {

const char* toString(Phase p) {
    switch (p) {
        case Phase::ToPickup: return "a_recogida";
        case Phase::ToDropoff: return "a_destino";
        case Phase::ToCharger: return "a_recarga";
        case Phase::Idle: return "en_espera";
    }
    return "?";
}

namespace {
// Las distancias reales por pasillos son mayores que la euclidiana.
constexpr float kDetour = 1.35f;
}  // namespace

float TaskSelector::energyForDistance(float meters, float mass) const {
    const auto& e = cfg_->energy;
    const auto& r = cfg_->robot;
    const float v = cfg_->sim.nominalSpeed;
    const float time = meters / v;
    // Base + rodadura; el factor 2 cubre aceleraciones, giros y desvíos.
    const float rolling = r.rollingResistance * mass * kGravity * v / e.efficiency;
    return 2.0f * (e.basePowerW + rolling) * time / 3600.0f;
}

Vec2 TaskSelector::nearestCharger(Vec2 p, const WarehouseMap& map) const {
    Vec2 best = map.chargers.front();
    for (const auto& c : map.chargers)
        if (distance(p, c) < distance(p, best)) best = c;
    return best;
}

float TaskSelector::score(const Task& t, const Context& ctx) const {
    static constexpr float kPriorityWeight[] = {1.0f, 2.0f, 4.0f, 8.0f};
    const float travel = (distance(ctx.robotPos, t.pickup) * kDetour + t.routeLength) / cfg_->sim.nominalSpeed;
    float urgency = 0.0f;
    if (t.hasDeadline()) {
        const double slack = t.deadline - ctx.now - travel;
        urgency = slack > 0 ? 20.0f * std::exp(-static_cast<float>(slack) / 60.0f) : 25.0f;
    }
    return 10.0f * kPriorityWeight[static_cast<int>(t.priority)] + urgency - 0.5f * travel;
}

Decision TaskSelector::decide(const Context& ctx, const TaskManager& tasks, const WarehouseMap& map) const {
    const auto& e = cfg_->energy;
    const float reserveWh = e.reserveFraction * e.capacityWh;

    if (ctx.carryingTask >= 0) {
        const Task& t = tasks.get(ctx.carryingTask);
        return {Phase::ToDropoff, t.id, t.destination};
    }

    const Task* best = nullptr;
    float bestScore = -1e9f;
    for (const auto& t : tasks.tasks()) {
        if (!t.open()) continue;
        const float s = score(t, ctx);
        if (s > bestScore) {
            bestScore = s;
            best = &t;
        }
    }

    const Vec2 charger = nearestCharger(ctx.robotPos, map);
    if (best) {
        const float meters = distance(ctx.robotPos, best->pickup) * kDetour + best->routeLength +
                             distance(best->destination, nearestCharger(best->destination, map)) * kDetour;
        const float loadMass = cfg_->loadTypes[static_cast<size_t>(best->loadType)].mass;
        const float need = energyForDistance(meters, ctx.totalMass + loadMass) + reserveWh;
        if (ctx.batteryWh < need) return {Phase::ToCharger, -1, charger};
        return {Phase::ToPickup, best->id, best->pickup};
    }
    // Sin tareas: estacionar en la estación más cercana (y recargar de paso).
    if (ctx.batteryWh < reserveWh * 2.0f) return {Phase::ToCharger, -1, charger};
    return {Phase::Idle, -1, charger};
}

}  // namespace wrl
