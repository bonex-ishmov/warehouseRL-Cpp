#include "warehouse/session/session.hpp"

#include <algorithm>
#include <chrono>
#include <exception>

namespace wrl {
namespace fs = std::filesystem;

const char* describe(EventKind kind) {
    switch (kind) {
        case EventKind::Pickup: return "recogió la tarea";
        case EventKind::Delivered: return "entregó a tiempo la tarea";
        case EventKind::DeliveredLate: return "entregó tarde la tarea";
        case EventKind::Collision: return "choque";
        case EventKind::LoadDrop: return "se le cayó la carga de la tarea";
        case EventKind::RechargeStart: return "empezó a recargar";
        case EventKind::RestrictedEntry: return "entró en zona restringida";
        case EventKind::CriticalOverdue: return "venció una tarea crítica";
    }
    return "";
}

void EpisodeHistory::clear() {
    time.clear();
    battery.clear();
    speed.clear();
    cumReturn.clear();
    delivered.clear();
    loadStress.clear();
    trail.clear();
    phase.clear();
    flags.clear();
    events.clear();
}

void EpisodeHistory::push(const WarehouseEnv& env, bool slipping) {
    time.push_back(static_cast<float>(env.time()));
    battery.push_back(env.batteryFraction());
    speed.push_back(env.measuredSpeed());
    cumReturn.push_back(static_cast<float>(env.metrics().totalReturn));
    delivered.push_back(static_cast<float>(env.metrics().delivered));
    loadStress.push_back(env.loadStress());
    trail.push_back(env.robot().position());
    phase.push_back(static_cast<uint8_t>(env.decision().phase));
    uint8_t f = 0;
    if (env.carryingTask() >= 0) f |= kCarrying;
    if (env.charging()) f |= kCharging;
    if (slipping) f |= kSlipping;
    flags.push_back(f);
}

AggregateStats summarize(const std::vector<EpisodeRecord>& records) {
    AggregateStats s;
    s.episodes = static_cast<int>(records.size());
    if (records.empty()) return s;
    for (const auto& r : records) {
        const auto& m = r.metrics;
        s.completion += m.completionRate();
        s.onTime += m.onTimeRate();
        s.harmfulCollisions += m.harmfulCollisions();
        s.energyWh += m.energyWh;
        s.duration += m.simTime;
        s.totalReturn += m.totalReturn;
        if (m.tasksTotal > 0 && m.delivered == m.tasksTotal) ++s.fullyCompleted;
    }
    const double n = static_cast<double>(records.size());
    s.completion /= n;
    s.onTime /= n;
    s.harmfulCollisions /= n;
    s.energyWh /= n;
    s.duration /= n;
    s.totalReturn /= n;
    return s;
}

// --- AgentRunner -------------------------------------------------------------

AgentRunner::AgentRunner(const ScenarioConfig& cfg, AgentSpec spec, std::unique_ptr<Policy> policy)
    : env_(cfg), spec_(std::move(spec)), policy_(std::move(policy)) {}

void AgentRunner::reset(uint64_t seed, const ScenarioConfig& cfg) {
    env_.reset(seed, cfg);
    policy_->reset(seed);
    history_.clear();
    history_.push(env_, false);
    last_ = {};
    error_.clear();
    recorded_ = false;
}

void AgentRunner::reset(uint64_t seed) {
    env_.reset(seed);
    policy_->reset(seed);
    history_.clear();
    history_.push(env_, false);
    last_ = {};
    error_.clear();
    recorded_ = false;
}

void AgentRunner::step() {
    if (finished()) return;
    const int carryingBefore = env_.carryingTask();
    const int slipBefore = env_.metrics().slipSteps;
    try {
        last_ = env_.step(policy_->act(env_));
    } catch (const std::exception& e) {
        // Un modelo incompatible con el escenario no debe tumbar la interfaz.
        error_ = e.what();
        return;
    }

    const auto& ev = last_.events;
    const Vec2 pos = env_.robot().position();
    const float t = static_cast<float>(env_.time());
    auto mark = [&](EventKind kind, int detail, int count = 1) {
        for (int i = 0; i < count; ++i) history_.events.push_back({t, pos, kind, detail});
    };
    mark(EventKind::Pickup, env_.carryingTask(), ev.pickups);
    mark(EventKind::Delivered, carryingBefore, ev.deliveriesOnTime);
    mark(EventKind::DeliveredLate, carryingBefore, ev.deliveriesLate);
    for (int s = 0; s < 3; ++s) mark(EventKind::Collision, s, ev.collisions[s]);
    mark(EventKind::LoadDrop, carryingBefore, ev.loadDrops);
    mark(EventKind::RechargeStart, -1, ev.rechargesStarted);
    mark(EventKind::RestrictedEntry, -1, ev.restrictedEntries);
    mark(EventKind::CriticalOverdue, -1, ev.criticalOverdue);
    history_.push(env_, env_.metrics().slipSteps > slipBefore);
    if (env_.done() && !recorded_) {
        records_.push_back({env_.seed(), env_.metrics()});
        recorded_ = true;
    }
}

// --- Session -----------------------------------------------------------------

Session::Session(PolicyFactory factory) : factory_(std::move(factory)) {}

void Session::setScenario(const fs::path& path) {
    ScenarioConfig cfg = loadScenario(path);
    setScenario(std::move(cfg));
    scenarioPath_ = path;
}

void Session::setScenarioProvider(ScenarioProvider provider) {
    provider_ = std::move(provider);
    scenarioPath_.clear();
    for (auto& r : runners_) r->clearRecords();
    reset(seed_);
}

void Session::setScenario(ScenarioConfig cfg) {
    provider_ = nullptr;
    scenario_ = std::move(cfg);
    scenarioPath_.clear();
    // Las políticas se recrean: algunas (DWA) toman los parámetros del robot
    // del primer entorno que ven y el escenario nuevo puede cambiarlos.
    std::vector<AgentSpec> specs;
    for (const auto& r : runners_) specs.push_back(r->spec());
    runners_.clear();
    for (auto& s : specs) runners_.push_back(std::make_unique<AgentRunner>(scenario_, s, factory_(s)));
    reset(seed_);
}

std::string Session::defaultLabel(const AgentSpec& spec) const {
    if (spec.kind == "sac") return "SAC · " + fs::path(spec.model).stem().string();
    if (spec.kind == "dwa") return "A* + DWA";
    if (spec.kind == "random") return "Aleatoria";
    if (spec.kind == "manual") return "Manual";
    return spec.kind;
}

void Session::addAgent(AgentSpec spec) {
    if (spec.label.empty()) spec.label = defaultLabel(spec);
    auto policy = factory_(spec);
    auto runner = std::make_unique<AgentRunner>(scenario_, std::move(spec), std::move(policy));
    // Una acción de prueba detecta aquí un modelo incompatible con el escenario
    // (tamaño de observación distinto) en lugar de a mitad del episodio.
    runner->reset(seed_);
    (void)runner->policy().act(runner->env());
    runner->reset(seed_);
    runners_.push_back(std::move(runner));
}

void Session::removeAgent(size_t index) {
    if (index < runners_.size()) runners_.erase(runners_.begin() + static_cast<std::ptrdiff_t>(index));
}

void Session::reset(uint64_t seed) {
    seed_ = seed;
    if (provider_) {
        scenario_ = provider_(seed);
        for (auto& r : runners_) r->reset(seed, scenario_);
    } else {
        for (auto& r : runners_) r->reset(seed);
    }
    accumulator_ = 0.0;
    finishedFor_ = 0.0;
}

bool Session::allFinished() const {
    return !runners_.empty() &&
           std::all_of(runners_.begin(), runners_.end(), [](const auto& r) { return r->finished(); });
}

void Session::setSpeed(double s) { speed_ = std::clamp(s, 0.0625, 256.0); }

void Session::setManualAction(Action a) {
    for (auto& r : runners_)
        if (auto* m = dynamic_cast<ManualPolicy*>(&r->policy())) m->set(a);
}

void Session::stepAll() {
    for (auto& r : runners_) r->step();
}

int Session::advance(double realSeconds) {
    if (runners_.empty()) return 0;
    int steps = 0;

    if (allFinished()) {
        accumulator_ = 0.0;
        if (autoAdvance_ && !paused_) {
            finishedFor_ += realSeconds;
            if (maxSpeed_ || finishedFor_ >= kAutoAdvanceDelay) nextEpisode();
        }
        return 0;
    }
    finishedFor_ = 0.0;

    if (stepRequested_) {
        stepRequested_ = false;
        stepAll();
        return 1;
    }
    if (paused_) {
        accumulator_ = 0.0;
        return 0;
    }

    const double dt = runners_.front()->env().controlDt();
    if (maxSpeed_) {
        // Tan rápido como se pueda sin bajar de ~60 cuadros por segundo.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(12);
        do {
            stepAll();
            ++steps;
        } while (!allFinished() && (steps & 7 || std::chrono::steady_clock::now() < deadline));
        return steps;
    }

    accumulator_ += std::min(realSeconds, 0.25) * speed_;
    constexpr int kBudget = 256;  // tope por cuadro para que la interfaz no se congele
    while (accumulator_ >= dt && steps < kBudget && !allFinished()) {
        accumulator_ -= dt;
        stepAll();
        ++steps;
    }
    if (steps == kBudget) accumulator_ = 0.0;
    return steps;
}

// --- Catálogo ----------------------------------------------------------------

namespace {

std::vector<CatalogEntry> scenariosIn(const fs::path& dir, const std::string& prefix) {
    std::vector<CatalogEntry> out;
    if (!fs::is_directory(dir)) return out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() != ".json") continue;
        CatalogEntry c;
        c.path = e.path();
        c.label = prefix + e.path().stem().string();
        try {
            const auto cfg = loadScenario(e.path());
            c.label = prefix + cfg.name;
            c.description = cfg.description;
        } catch (const std::exception& ex) {
            c.description = std::string("no se pudo leer: ") + ex.what();
        }
        out.push_back(std::move(c));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.path < b.path; });
    return out;
}

}  // namespace

std::vector<CatalogEntry> listScenarios(const fs::path& root) {
    auto out = scenariosIn(root, "");
    auto train = scenariosIn(root / "train", "currículo · ");
    out.insert(out.end(), train.begin(), train.end());
    return out;
}

std::vector<CatalogEntry> listModels(const std::vector<fs::path>& dirs, const fs::path& base) {
    std::vector<CatalogEntry> out;
    for (const auto& dir : dirs) {
        if (!fs::is_directory(dir)) continue;
        for (const auto& e : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
            if (!e.is_regular_file() || e.path().extension() != ".pt") continue;
            // Los checkpoints completos incluyen críticos y optimizadores, pero
            // también el actor: se pueden cargar igual que un modelo final.
            CatalogEntry c;
            c.path = e.path();
            std::error_code ec;
            auto rel = fs::relative(e.path(), base, ec);
            c.label = ec ? e.path().string() : rel.string();
            out.push_back(std::move(c));
        }
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.label < b.label; });
    return out;
}

}  // namespace wrl
