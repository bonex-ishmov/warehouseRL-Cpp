#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "warehouse/env/policies.hpp"
#include "warehouse/session/history.hpp"

namespace wrl {

/// Qué agente ocupa un puesto de la sesión.
struct AgentSpec {
    std::string kind;   ///< "sac", "dwa", "random" o "manual"
    std::string model;  ///< ruta del modelo .pt (solo "sac")
    std::string label;  ///< nombre visible; vacío = se deduce de kind y model
};

/// Crea la política de un puesto. La aplicación la inyecta para que el núcleo
/// no dependa de LibTorch (la política SAC vive en wrl_agent).
using PolicyFactory = std::function<std::unique_ptr<Policy>(const AgentSpec&)>;

struct EpisodeRecord {
    uint64_t seed = 0;
    EpisodeMetrics metrics;
};

/// Medias sobre los episodios terminados de un agente.
struct AggregateStats {
    int episodes = 0;
    double completion = 0.0;  ///< fracción media de tareas entregadas
    double onTime = 0.0;
    double harmfulCollisions = 0.0;
    double energyWh = 0.0;
    double duration = 0.0;
    double totalReturn = 0.0;
    int fullyCompleted = 0;  ///< episodios con todas las tareas entregadas
};
AggregateStats summarize(const std::vector<EpisodeRecord>& records);

/// Un agente con su propio entorno. Todos los puestos de una sesión usan el
/// mismo escenario y la misma semilla para poder compararlos.
class AgentRunner {
public:
    AgentRunner(const ScenarioConfig& cfg, AgentSpec spec, std::unique_ptr<Policy> policy);

    void reset(uint64_t seed);
    void reset(uint64_t seed, const ScenarioConfig& cfg);
    /// Un paso de control. No hace nada si el episodio terminó o hubo un error.
    void step();
    bool finished() const { return env_.done() || !error_.empty(); }

    const WarehouseEnv& env() const { return env_; }
    const AgentSpec& spec() const { return spec_; }
    Policy& policy() { return *policy_; }
    const EpisodeHistory& history() const { return history_; }
    const std::vector<EpisodeRecord>& records() const { return records_; }
    const StepResult& lastResult() const { return last_; }
    const std::string& error() const { return error_; }
    void clearRecords() { records_.clear(); }

private:
    WarehouseEnv env_;
    AgentSpec spec_;
    std::unique_ptr<Policy> policy_;
    EpisodeHistory history_;
    std::vector<EpisodeRecord> records_;
    StepResult last_;
    std::string error_;
    bool recorded_ = false;
};

/// Modelo de la interfaz de visualización: escenario, agentes y reproducción.
/// No sabe nada de gráficos; la interfaz solo lee su estado y le envía órdenes.
class Session {
public:
    explicit Session(PolicyFactory factory);

    /// Cambia el escenario; conserva los agentes y borra sus estadísticas.
    void setScenario(const std::filesystem::path& path);
    void setScenario(ScenarioConfig cfg);
    /// Escenario nuevo en cada episodio (generador). Las estadísticas se acumulan
    /// entre episodios; setScenario() lo desactiva.
    using ScenarioProvider = std::function<ScenarioConfig(uint64_t seed)>;
    void setScenarioProvider(ScenarioProvider provider);
    bool hasScenarioProvider() const { return static_cast<bool>(provider_); }
    const ScenarioConfig& scenario() const { return scenario_; }
    const std::filesystem::path& scenarioPath() const { return scenarioPath_; }

    /// Lanza std::exception si la política no se puede crear (p. ej. modelo inválido).
    void addAgent(AgentSpec spec);
    void removeAgent(size_t index);
    size_t agentCount() const { return runners_.size(); }
    AgentRunner& agent(size_t i) { return *runners_[i]; }
    const AgentRunner& agent(size_t i) const { return *runners_[i]; }

    void reset(uint64_t seed);
    void nextEpisode() { reset(seed_ + 1); }
    uint64_t seed() const { return seed_; }
    bool allFinished() const;

    // --- Reproducción ---
    void setPaused(bool p) { paused_ = p; }
    bool paused() const { return paused_; }
    void setSpeed(double s);  ///< segundos simulados por segundo real
    double speed() const { return speed_; }
    void setMaxSpeed(bool on) { maxSpeed_ = on; }
    bool maxSpeed() const { return maxSpeed_; }
    void setAutoAdvance(bool on) { autoAdvance_ = on; }
    bool autoAdvance() const { return autoAdvance_; }
    void requestStep() { stepRequested_ = true; }
    void setManualAction(Action a);

    /// Avanza la simulación según el tiempo real transcurrido. Devuelve los
    /// pasos de control ejecutados (por agente).
    int advance(double realSeconds);
    /// Un paso de control para todos los agentes que no hayan terminado.
    void stepAll();

    static constexpr double kAutoAdvanceDelay = 1.5;  ///< s reales mostrando el final

private:
    std::string defaultLabel(const AgentSpec& spec) const;

    PolicyFactory factory_;
    ScenarioProvider provider_;
    ScenarioConfig scenario_;
    std::filesystem::path scenarioPath_;
    std::vector<std::unique_ptr<AgentRunner>> runners_;
    uint64_t seed_ = 1;
    bool paused_ = false;
    double speed_ = 1.0;
    bool maxSpeed_ = false;
    bool autoAdvance_ = true;
    bool stepRequested_ = false;
    double accumulator_ = 0.0;
    double finishedFor_ = 0.0;
};

/// Archivo encontrado en disco con un nombre para mostrar.
struct CatalogEntry {
    std::string label;
    std::string description;
    std::filesystem::path path;
};

/// Escenarios de evaluación y, a continuación, las etapas del currículo.
std::vector<CatalogEntry> listScenarios(const std::filesystem::path& root);
/// Modelos .pt dentro de las carpetas dadas (recursivo), con la ruta relativa a `base`.
std::vector<CatalogEntry> listModels(const std::vector<std::filesystem::path>& dirs, const std::filesystem::path& base);

}  // namespace wrl
