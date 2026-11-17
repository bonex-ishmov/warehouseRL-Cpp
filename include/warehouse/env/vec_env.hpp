#pragma once

#include <atomic>
#include <barrier>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "warehouse/env/warehouse_env.hpp"

namespace wrl {

/// N entornos que avanzan en paralelo sobre un grupo fijo de hilos.
///
/// Los hilos se crean una vez y se sincronizan con std::barrier, así que cada
/// paso cuesta dos sincronizaciones y ninguna creación de hilos. Los buffers de
/// observaciones, recompensas y finales son contiguos para copiarlos de una
/// vez a un tensor. Los entornos terminados se reinician automáticamente; la
/// observación final queda en finalObservations() para el bootstrap de SAC.
class VecEnv {
public:
    static constexpr int kMaxEnvs = 100;

    VecEnv(const ScenarioConfig& cfg, int numEnvs, int numThreads, uint64_t seed);
    ~VecEnv();
    VecEnv(const VecEnv&) = delete;
    VecEnv& operator=(const VecEnv&) = delete;

    /// Cambia el escenario (currículo). Cada entorno se reconstruye y reinicia.
    void setScenario(const ScenarioConfig& cfg);
    /// Escenario distinto en cada reinicio (generador aleatorio). Se llama desde
    /// los hilos de simulación, así que debe ser seguro para llamadas concurrentes
    /// con distinto `env`. Reinicia todos los entornos.
    using ScenarioProvider = std::function<ScenarioConfig(int env, uint64_t seed)>;
    void setScenarioProvider(ScenarioProvider provider);
    void resetAll();
    /// actions: numEnvs * 2 valores en [-1, 1].
    void step(const float* actions);
    /// Ejecuta fn(i) para cada entorno usando el mismo grupo de hilos.
    void parallelFor(const std::function<void(int)>& fn);

    int size() const { return static_cast<int>(envs_.size()); }
    int observationSize() const { return obsSize_; }
    int threads() const { return static_cast<int>(workers_.size()) + 1; }

    const float* observations() const { return obs_.data(); }
    const float* finalObservations() const { return finalObs_.data(); }
    const float* rewards() const { return rewards_.data(); }
    const uint8_t* terminated() const { return terminated_.data(); }
    const uint8_t* truncated() const { return truncated_.data(); }

    /// Métricas de los episodios que terminaron en el último paso.
    const std::vector<EpisodeMetrics>& finishedEpisodes() const { return finished_; }
    /// Índice del entorno de cada elemento de finishedEpisodes().
    const std::vector<int>& finishedEnvs() const { return finishedEnvs_; }
    WarehouseEnv& env(int i) { return *envs_[static_cast<size_t>(i)]; }

private:
    void workerLoop(int worker);
    void runSlice(int worker);
    void stepEnv(int i);
    uint64_t nextSeed(int i);
    void resetEnv(int i);

    std::vector<std::unique_ptr<WarehouseEnv>> envs_;
    std::vector<uint64_t> episodeCount_;
    uint64_t baseSeed_;
    int obsSize_ = 0;
    std::vector<float> obs_, finalObs_, rewards_;
    std::vector<uint8_t> terminated_, truncated_;
    std::vector<EpisodeMetrics> finishedPerEnv_;
    std::vector<uint8_t> finishedFlag_;
    std::vector<EpisodeMetrics> finished_;
    std::vector<int> finishedEnvs_;
    ScenarioProvider provider_;

    const float* actions_ = nullptr;
    std::function<void(int)> job_;
    int numWorkers_ = 0;
    std::atomic<bool> stop_{false};
    std::unique_ptr<std::barrier<>> start_, end_;
    std::vector<std::jthread> workers_;
};

}  // namespace wrl
