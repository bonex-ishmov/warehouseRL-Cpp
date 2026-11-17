#include "warehouse/env/vec_env.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace wrl {

VecEnv::VecEnv(const ScenarioConfig& cfg, int numEnvs, int numThreads, uint64_t seed) : baseSeed_(seed) {
    // Box2D admite 128 mundos simultáneos; se deja margen para el proceso.
    if (numEnvs > kMaxEnvs) throw std::runtime_error("VecEnv admite como máximo " + std::to_string(kMaxEnvs) + " entornos");
    numEnvs = std::max(1, numEnvs);
    numThreads = std::clamp(numThreads, 1, numEnvs);
    obsSize_ = WarehouseEnv::observationSize(cfg.sensors);
    for (int i = 0; i < numEnvs; ++i) envs_.push_back(std::make_unique<WarehouseEnv>(cfg));
    episodeCount_.assign(static_cast<size_t>(numEnvs), 0);
    const size_t n = static_cast<size_t>(numEnvs);
    obs_.assign(n * static_cast<size_t>(obsSize_), 0.0f);
    finalObs_ = obs_;
    rewards_.assign(n, 0.0f);
    terminated_.assign(n, 0);
    truncated_.assign(n, 0);
    finishedPerEnv_.resize(n);
    finishedFlag_.assign(n, 0);

    // El hilo principal también trabaja: se crean numThreads - 1 hilos auxiliares.
    numWorkers_ = numThreads - 1;
    start_ = std::make_unique<std::barrier<>>(numThreads);
    end_ = std::make_unique<std::barrier<>>(numThreads);
    for (int w = 0; w < numWorkers_; ++w) workers_.emplace_back([this, w] { workerLoop(w + 1); });
    resetAll();
}

VecEnv::~VecEnv() {
    stop_ = true;
    if (numWorkers_ > 0) start_->arrive_and_wait();  // despierta a los hilos para que salgan
    workers_.clear();
}

uint64_t VecEnv::nextSeed(int i) {
    return mixSeed(baseSeed_, static_cast<uint64_t>(i) * 1'000'003ull + episodeCount_[static_cast<size_t>(i)]++);
}

void VecEnv::workerLoop(int worker) {
    while (true) {
        start_->arrive_and_wait();
        if (stop_) return;
        runSlice(worker);
        end_->arrive_and_wait();
    }
}

void VecEnv::runSlice(int worker) {
    const int threads = numWorkers_ + 1;
    for (int i = worker; i < size(); i += threads) job_(i);
}

void VecEnv::setScenario(const ScenarioConfig& cfg) {
    provider_ = nullptr;
    if (WarehouseEnv::observationSize(cfg.sensors) != obsSize_)
        throw std::runtime_error("el nuevo escenario cambia el tamaño de la observación");
    for (auto& e : envs_) e = std::make_unique<WarehouseEnv>(cfg);
    resetAll();
}

void VecEnv::setScenarioProvider(ScenarioProvider provider) {
    provider_ = std::move(provider);
    resetAll();
}

void VecEnv::resetEnv(int i) {
    const uint64_t seed = nextSeed(i);
    auto& env = *envs_[static_cast<size_t>(i)];
    if (provider_) env.reset(seed, provider_(i, seed));
    else env.reset(seed);
}

void VecEnv::resetAll() {
    job_ = [this](int i) {
        resetEnv(i);
        const auto& o = envs_[static_cast<size_t>(i)]->observation();
        std::memcpy(obs_.data() + static_cast<size_t>(i * obsSize_), o.data(), o.size() * sizeof(float));
    };
    if (numWorkers_ > 0) start_->arrive_and_wait();
    runSlice(0);
    if (numWorkers_ > 0) end_->arrive_and_wait();
}

void VecEnv::stepEnv(int i) {
    const size_t k = static_cast<size_t>(i);
    WarehouseEnv& env = *envs_[k];
    const Action a{actions_[2 * k], actions_[2 * k + 1]};
    const StepResult r = env.step(a);
    rewards_[k] = r.reward;
    terminated_[k] = r.terminated;
    truncated_[k] = r.truncated;
    float* dst = obs_.data() + k * static_cast<size_t>(obsSize_);
    if (r.done()) {
        std::memcpy(finalObs_.data() + k * static_cast<size_t>(obsSize_), env.observation().data(),
                    static_cast<size_t>(obsSize_) * sizeof(float));
        finishedPerEnv_[k] = env.metrics();
        finishedFlag_[k] = 1;
        resetEnv(i);
    } else {
        finishedFlag_[k] = 0;
    }
    std::memcpy(dst, env.observation().data(), static_cast<size_t>(obsSize_) * sizeof(float));
}

void VecEnv::parallelFor(const std::function<void(int)>& fn) {
    job_ = fn;
    if (numWorkers_ > 0) start_->arrive_and_wait();
    runSlice(0);
    if (numWorkers_ > 0) end_->arrive_and_wait();
}

void VecEnv::step(const float* actions) {
    actions_ = actions;
    job_ = [this](int i) { stepEnv(i); };
    if (numWorkers_ > 0) start_->arrive_and_wait();
    runSlice(0);
    if (numWorkers_ > 0) end_->arrive_and_wait();

    finished_.clear();
    finishedEnvs_.clear();
    for (size_t k = 0; k < envs_.size(); ++k) {
        if (!finishedFlag_[k]) continue;
        finished_.push_back(finishedPerEnv_[k]);
        finishedEnvs_.push_back(static_cast<int>(k));
    }
}

}  // namespace wrl
