#include "warehouse/sim/scenario_generator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace wrl {
namespace {

float lerp(float a, float b, float t) { return a + (b - a) * std::clamp(t, 0.0f, 1.0f); }

// Mismo modelo que TaskSelector::energyForDistance: base + rodadura, x2 por
// aceleraciones, giros y desvíos.
float energyWh(const ScenarioConfig& c, float meters, float mass) {
    const auto& e = c.energy;
    const float v = c.sim.nominalSpeed;
    const float rolling = c.robot.rollingResistance * mass * kGravity * v / e.efficiency;
    return 2.0f * (e.basePowerW + rolling) * (meters / v) / 3600.0f;
}

float heaviestLoad(const ScenarioConfig& c) {
    float m = 0.0f;
    for (const auto& l : c.loadTypes) m = std::max(m, l.mass);
    return m;
}

}  // namespace

const char* toString(Skill s) {
    switch (s) {
        case Skill::Layout: return "mapa";
        case Skill::Tasks: return "tareas";
        case Skill::Energy: return "energia";
        case Skill::Loads: return "cargas";
        case Skill::Obstacles: return "obstaculos";
        case Skill::Terrain: return "terreno";
    }
    return "?";
}

std::string Difficulty::str() const {
    std::string out;
    char buf[32];
    for (int k = 0; k < kSkills; ++k) {
        std::snprintf(buf, sizeof buf, "%s%s %.2f", k ? " " : "", toString(static_cast<Skill>(k)), static_cast<double>(v[static_cast<size_t>(k)]));
        out += buf;
    }
    return out;
}

ScenarioGenerator::ScenarioGenerator(ScenarioConfig base) : base_(std::move(base)) {
    if (base_.loadTypes.empty()) throw std::runtime_error("el generador necesita tipos de carga en la configuración base");
}

float ScenarioGenerator::minimumCapacityWh(float width, float height) const {
    // Viaje largo pero razonable: 2.2 diagonales (ir a recoger, llevar y volver
    // a un cargador) con la carga más pesada, más la reserva y un 5 % de margen.
    const float diag = std::sqrt(width * width + height * height);
    const float need = energyWh(base_, 2.2f * diag, base_.robot.mass + heaviestLoad(base_));
    return std::max(2.5f, need / (1.0f - base_.energy.reserveFraction - 0.05f));
}

ScenarioConfig ScenarioGenerator::generate(const Difficulty& d, uint64_t seed) const {
    Rng rng(mixSeed(seed, 0x5CE7A110ull));
    ScenarioConfig c = base_;
    c.raw = Json();
    c.acceptance = Json::object();
    c.fixedObstacles.clear();

    // --- Mapa: almacenes más grandes y pasillos más estrechos al subir. ---
    const float dl = d[Skill::Layout];
    auto& p = c.map.proc;
    c.map.procedural = true;
    p.width = std::clamp(std::round(lerp(18.0f, 28.0f, dl) + rng.uniform(-2.0f, 2.0f)), 16.0f, 30.0f);
    p.height = std::clamp(std::round(lerp(18.0f, 28.0f, dl) + rng.uniform(-2.0f, 2.0f)), 16.0f, 30.0f);
    // Regla: el pasillo libre (descontando el inflado del robot a ambos lados)
    // debe medir al menos 1.2 m para que A* y el robot quepan sin rozar.
    const float minAisle = 2.0f * c.sim.inflation + 1.2f;
    // El nivel máximo se parece a E5 (2.6 m); E6 (2.4 m) queda fuera del rango de entrenamiento.
    p.aisleWidth = std::max(minAisle, lerp(2.9f, 2.5f, dl) + rng.uniform(-0.1f, 0.1f));
    p.crossAisle = std::max(minAisle, lerp(3.0f, 2.5f, dl) + rng.uniform(-0.1f, 0.2f));
    p.shelfDepth = rng.uniform(0.8f, 1.1f);
    p.shelfLength = rng.uniform(3.5f, lerp(5.0f, 7.0f, dl));
    p.margin = 3.0f;  // franjas de muelles y cargadores
    p.removeShelfProb = rng.uniform(0.0f, 0.1f + 0.2f * dl);
    const int maxDocks = std::max(2, static_cast<int>((p.width - 5.0f) / 1.6f));
    p.dropoffs = std::min(maxDocks, rng.uniformInt(2, 5));
    p.chargers = dl < 0.3f ? 2 : rng.uniformInt(1, 3);
    c.map.width = p.width;
    c.map.height = p.height;
    const float diag = std::sqrt(p.width * p.width + p.height * p.height);

    // --- Terreno y física: zonas resbaladizas, zona restringida y aleatorización. ---
    const float dt = d[Skill::Terrain];
    p.frictionZones = static_cast<int>(std::round(dt * 3.0f * rng.uniform(0.5f, 1.0f)));
    p.frictionMin = lerp(0.6f, 0.3f, dt);
    p.frictionMax = lerp(0.7f, 0.5f, dt);
    p.restrictedZones = dt > 0.5f && rng.bernoulli(0.7f) ? 1 : 0;
    auto& r = c.randomization;
    r.enabled = true;
    r.friction = lerp(0.05f, 0.3f, dt);
    r.mass = lerp(0.05f, 0.15f, dt);
    r.motorForce = lerp(0.05f, 0.1f, dt);
    r.lidarNoiseMax = lerp(0.01f, 0.05f, dt);
    r.obstacleSpeed = 0.2f;

    // --- Cargas: se habilitan tipos más pesados y frágiles. ---
    const float dc = d[Skill::Loads];
    c.tasks.loadTypes.clear();
    const char* order[] = {"liviana", "media", "pesada", "fragil"};
    const float unlock[] = {0.0f, 0.25f, 0.5f, 0.75f};
    for (int k = 0; k < 4; ++k) {
        bool exists = false;
        for (const auto& l : c.loadTypes) exists |= l.name == order[k];
        if (exists && dc >= unlock[k]) c.tasks.loadTypes.push_back(order[k]);
    }
    if (c.tasks.loadTypes.empty()) c.tasks.loadTypes.push_back(c.loadTypes.front().name);

    // --- Tareas: cantidad, llegadas, prioridades y plazos. ---
    const float dk = d[Skill::Tasks];
    auto& t = c.tasks;
    t.count = 1 + static_cast<int>(std::round(dk * 14.0f * rng.uniform(0.5f, 1.0f)));
    t.initial = std::min(t.count, 1 + static_cast<int>(std::round(2.0f * dk)));
    t.arrivalInterval = rng.uniform(20.0f, 60.0f);
    t.priorityWeights = {1.0f - 0.7f * dk, 0.3f * dk, 0.25f * dk, 0.15f * std::max(0.0f, (dk - 0.4f) / 0.6f)};
    t.deadlines = dk >= 0.35f && rng.bernoulli(0.8f);
    // Regla: la holgura nunca baja de 2x el tiempo estimado (como E5; la línea base cumple ~84 % con 2-3.5x).
    t.slackMin = lerp(3.5f, 2.0f, dk);
    t.slackMax = t.slackMin + 1.5f;
    t.deadlineBase = 45.0f;
    if (t.deadlines) {
        // Regla: con plazos, las tareas no llegan mucho más rápido de lo que se
        // tarda en atender una. En lo más difícil se permite algo de cola (como en
        // E5): algunas tareas vencen y hay que entregarlas igual, tarde.
        const float serviceTime = 1.35f * diag / 0.9f + 4.0f;
        const float backlog = dk > 0.7f ? lerp(0.9f, 0.7f, (dk - 0.7f) / 0.3f) : 0.9f;
        t.arrivalInterval = std::max(t.arrivalInterval, backlog * serviceTime);
    }

    // --- Energía: batería más pequeña y carga inicial menor, sin bajar del mínimo seguro. ---
    const float de = d[Skill::Energy];
    auto& e = c.energy;
    const float minCap = minimumCapacityWh(p.width, p.height);
    e.capacityWh = std::max(minCap, lerp(25.0f, minCap, de) * rng.uniform(0.9f, 1.1f));
    // Regla: con la carga inicial se debe poder llegar a un cargador desde cualquier punto.
    const float toCharger = energyWh(c, 1.35f * diag, c.robot.mass) / e.capacityWh;
    e.initialFraction = std::clamp(lerp(1.0f, 0.55f, de) * rng.uniform(0.9f, 1.0f), e.reserveFraction + toCharger + 0.1f, 1.0f);

    // --- Obstáculos móviles: limitados por la superficie para no bloquear el almacén. ---
    const float dob = d[Skill::Obstacles];
    auto& o = c.obstacles;
    // Regla: como máximo uno por cada ~80 m² (E4 tiene hasta 8 en 576 m²), y nunca más de 7.
    const int maxByArea = std::min(7, static_cast<int>(p.width * p.height / 80.0f));
    o.maxCount = static_cast<int>(std::round(dob * static_cast<float>(maxByArea)));
    o.minCount = std::max(0, o.maxCount - 3);
    o.forkliftFraction = rng.uniform(0.2f, 0.6f);
    o.speedMin = lerp(0.4f, 0.6f, dob);
    o.speedMax = lerp(0.7f, 1.1f, dob);
    o.stochasticFraction = 0.5f;

    // --- Tiempo máximo proporcional al trabajo pedido. ---
    const float perTask = 1.35f * diag / c.sim.nominalSpeed + 2.0f * c.sim.dockTime;
    const float arrivals = static_cast<float>(t.count - t.initial) * t.arrivalInterval;
    const float charging = de > 0.2f ? static_cast<float>(t.count) * 30.0f : 0.0f;
    c.sim.maxTime = std::max(180.0f, arrivals + 1.3f * static_cast<float>(t.count) * perTask + charging);

    c.name = "Generado";
    c.description = d.str();
    validate(c);
    return c;
}

// --- Currículo adaptativo -------------------------------------------------------

AdaptiveCurriculum::AdaptiveCurriculum() : AdaptiveCurriculum(Params{}) {}

AdaptiveCurriculum::AdaptiveCurriculum(Params p) : p_(p) { levels_ = Difficulty::uniform(p.startLevel); }

AdaptiveCurriculum::Sample AdaptiveCurriculum::sample(Rng& rng) const {
    Sample s;
    if (rng.bernoulli(p_.jointProb)) {
        // Una sola fracción para todas, con algo de ruido: si una es difícil, las demás también.
        const float u = rng.uniform(0.0f, 1.0f);
        for (int k = 0; k < kSkills; ++k)
            s.difficulty.v[static_cast<size_t>(k)] = levels_.v[static_cast<size_t>(k)] * std::clamp(u + rng.uniform(-0.15f, 0.15f), 0.0f, 1.0f);
    } else {
        for (int k = 0; k < kSkills; ++k) s.difficulty.v[static_cast<size_t>(k)] = rng.uniform(0.0f, levels_.v[static_cast<size_t>(k)]);
    }
    if (rng.bernoulli(p_.frontierProb)) {
        // Solo las dimensiones que aún no llegaron al máximo pueden empujarse.
        std::array<int, kSkills> open{};
        int n = 0;
        for (int k = 0; k < kSkills; ++k)
            if (levels_.v[static_cast<size_t>(k)] < 1.0f) open[static_cast<size_t>(n++)] = k;
        if (n > 0) {
            const int k = open[static_cast<size_t>(rng.uniformInt(0, n - 1))];
            const float lv = levels_.v[static_cast<size_t>(k)];
            s.difficulty.v[static_cast<size_t>(k)] = std::min(1.0f, lv + rng.uniform(0.0f, p_.frontierWidth));
            s.frontier = k;
        }
    }
    return s;
}

void AdaptiveCurriculum::report(const Sample& s, double score) {
    if (s.frontier < 0) return;
    const auto k = static_cast<size_t>(s.frontier);
    sum_[k] += score;
    if (++count_[k] < p_.window) return;
    const double mean = sum_[k] / count_[k];
    if (mean >= p_.raiseAt) levels_.v[k] = std::min(1.0f, levels_.v[k] + p_.step);
    else if (mean < p_.lowerAt) levels_.v[k] = std::max(0.0f, levels_.v[k] - p_.step);
    sum_[k] = 0.0;
    count_[k] = 0;
}

float AdaptiveCurriculum::progress() const {
    float s = 0.0f;
    for (float x : levels_.v) s += x;
    return s / kSkills;
}

}  // namespace wrl
