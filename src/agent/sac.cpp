#include "warehouse/agent/sac.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <dlfcn.h>

#include <torch/cuda.h>

#include "warehouse/agent/replay_buffer.hpp"

#if WRL_CUDA_GRAPHS
#include <ATen/cuda/CUDAGraph.h>
#include <c10/cuda/CUDAGuard.h>
#include <c10/cuda/CUDAStream.h>
#endif

namespace wrl {

namespace {
constexpr float kLogStdMin = -5.0f;
constexpr float kLogStdMax = 2.0f;
const float kHalfLog2Pi = 0.5f * std::log(2.0f * 3.14159265f);

template <typename T>
void read(const Json& j, const char* key, T& out) {
    if (auto it = j.find(key); it != j.end()) out = it->get<T>();
}
}  // namespace

SacConfig SacConfig::fromJson(const Json& j) {
    SacConfig c;
    read(j, "hidden", c.hidden);
    read(j, "gamma", c.gamma);
    read(j, "tau", c.tau);
    read(j, "actorLr", c.actorLr);
    read(j, "criticLr", c.criticLr);
    read(j, "alphaLr", c.alphaLr);
    read(j, "initAlpha", c.initAlpha);
    read(j, "autoAlpha", c.autoAlpha);
    read(j, "targetEntropy", c.targetEntropy);
    read(j, "minAlpha", c.minAlpha);
    read(j, "maxGradNorm", c.maxGradNorm);
    read(j, "rewardScale", c.rewardScale);
    read(j, "batchSize", c.batchSize);
    read(j, "cudaGraph", c.cudaGraph);
    read(j, "criticLayerNorm", c.criticLayerNorm);
    read(j, "tf32", c.tf32);
    return c;
}

torch::Device resolveDevice(const std::string& name) {
    if (name == "auto") return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
    if (name == "cuda" && !torch::cuda::is_available()) throw std::runtime_error("CUDA no está disponible");
    return torch::Device(name);
}

bool preferBlockingCudaSync() {
    // Se busca el símbolo en el runtime de CUDA que ya trae LibTorch, para no
    // depender de las cabeceras del CUDA Toolkit (ver cmake/LibTorch.cmake).
    using SetFlags = int (*)(unsigned int);
    constexpr unsigned int kScheduleBlockingSync = 0x04;  // cudaDeviceScheduleBlockingSync
    auto fn = reinterpret_cast<SetFlags>(dlsym(RTLD_DEFAULT, "cudaSetDeviceFlags"));
    return fn && fn(kScheduleBlockingSync) == 0;
}

// ---------------------------------------------------------------- Actor

ActorImpl::ActorImpl(int obsDim, int actDim, int hidden) {
    l1 = register_module("l1", torch::nn::Linear(obsDim, hidden));
    l2 = register_module("l2", torch::nn::Linear(hidden, hidden));
    mean = register_module("mean", torch::nn::Linear(hidden, actDim));
    logStd = register_module("logStd", torch::nn::Linear(hidden, actDim));
}

std::pair<torch::Tensor, torch::Tensor> ActorImpl::forward(const torch::Tensor& obs) {
    auto h = torch::relu(l1(obs));
    h = torch::relu(l2(h));
    return {mean(h), torch::clamp(logStd(h), kLogStdMin, kLogStdMax)};
}

std::pair<torch::Tensor, torch::Tensor> ActorImpl::sample(const torch::Tensor& obs) {
    auto [mu, logSigma] = forward(obs);
    const auto std = logSigma.exp();
    const auto eps = torch::randn_like(mu);
    const auto u = mu + std * eps;
    const auto a = torch::tanh(u);
    // log N(u; mu, std) - sum log(1 - tanh(u)^2), forma estable: 2 (log 2 - u - softplus(-2u)).
    auto logp = (-0.5f * eps.pow(2) - logSigma - kHalfLog2Pi).sum(-1, true);
    logp = logp - (2.0f * (std::log(2.0f) - u - torch::softplus(-2.0f * u))).sum(-1, true);
    return {a, logp};
}

torch::Tensor ActorImpl::deterministic(const torch::Tensor& obs) { return torch::tanh(forward(obs).first); }

// ---------------------------------------------------------------- Crítico

EnsembleLinearImpl::EnsembleLinearImpl(int ensemble, int in, int out) {
    const float bound = 1.0f / std::sqrt(static_cast<float>(in));
    W = register_parameter("W", torch::empty({ensemble, in, out}).uniform_(-bound, bound));
    b = register_parameter("b", torch::empty({ensemble, 1, out}).uniform_(-bound, bound));
}

torch::Tensor EnsembleLinearImpl::forward(const torch::Tensor& x) { return torch::baddbmm(b, x, W); }

CriticImpl::CriticImpl(int obsDim, int actDim, int hiddenSize, int ensembleSize, bool ln)
    : ensemble(ensembleSize), hidden(hiddenSize), layerNorm(ln) {
    l1 = register_module("l1", EnsembleLinear(ensemble, obsDim + actDim, hidden));
    l2 = register_module("l2", EnsembleLinear(ensemble, hidden, hidden));
    l3 = register_module("l3", EnsembleLinear(ensemble, hidden, 1));
}

torch::Tensor CriticImpl::forward(const torch::Tensor& obs, const torch::Tensor& act) {
    auto x = torch::cat({obs, act}, -1).unsqueeze(0).expand({ensemble, -1, -1});
    auto norm = [this](const torch::Tensor& t) { return layerNorm ? torch::layer_norm(t, {hidden}) : t; };
    x = torch::relu(norm(l1(x)));
    x = torch::relu(norm(l2(x)));
    return l3(x);
}

// ---------------------------------------------------------------- Agente

SacAgent::SacAgent(int obsDim, int actDim, const SacConfig& cfg, torch::Device device)
    : obsDim_(obsDim),
      actDim_(actDim),
      cfg_(cfg),
      device_(device),
      actor_(obsDim, actDim, cfg.hidden),
      critic_(obsDim, actDim, cfg.hidden, 2, cfg.criticLayerNorm),
      criticTarget_(obsDim, actDim, cfg.hidden, 2, cfg.criticLayerNorm) {
    if (device_.is_cuda()) at::globalContext().setAllowTF32CuBLAS(cfg.tf32);
    actor_->to(device_);
    critic_->to(device_);
    criticTarget_->to(device_);
    {
        torch::NoGradGuard ng;
        auto src = critic_->parameters();
        auto dst = criticTarget_->parameters();
        for (size_t i = 0; i < src.size(); ++i) dst[i].copy_(src[i]);
    }
    for (auto& p : criticTarget_->parameters()) p.set_requires_grad(false);
    targetParams_ = criticTarget_->parameters();
    sourceParams_ = critic_->parameters();

    logAlpha_ = torch::full({1}, std::log(cfg.initAlpha), torch::TensorOptions().device(device_)).set_requires_grad(true);
    actorOpt_ = std::make_unique<ForeachAdam>(actor_->parameters(), cfg.actorLr, cfg.maxGradNorm);
    criticOpt_ = std::make_unique<ForeachAdam>(critic_->parameters(), cfg.criticLr, cfg.maxGradNorm);
    alphaOpt_ = std::make_unique<ForeachAdam>(std::vector<torch::Tensor>{logAlpha_}, cfg.alphaLr);
}

ForeachAdam::ForeachAdam(std::vector<torch::Tensor> params, float lr, float maxGradNorm, float beta1, float beta2, float eps)
    : params_(std::move(params)), lr_(lr), maxNorm_(maxGradNorm), b1_(beta1), b2_(beta2), eps_(eps) {
    for (const auto& p : params_) {
        m_.push_back(torch::zeros_like(p));
        v_.push_back(torch::zeros_like(p));
    }
    stepCount_ = torch::zeros({}, params_.front().options().requires_grad(false));  // escalar 0-dim
}

void ForeachAdam::zeroGrad() {
    for (auto& p : params_) p.mutable_grad().reset();
}

void ForeachAdam::step() {
    torch::NoGradGuard ng;
    std::vector<torch::Tensor> grads;
    grads.reserve(params_.size());
    for (const auto& p : params_) grads.push_back(p.grad().defined() ? p.grad() : torch::zeros_like(p));
    if (maxNorm_ > 0.0f) {
        // Recorte por norma global calculado en el dispositivo (sin .item(): capturable).
        const auto norms = at::_foreach_norm(grads);
        const auto total = torch::stack(norms).norm();
        const auto coef = torch::clamp_max(maxNorm_ / (total + 1e-6), 1.0);
        at::_foreach_mul_(grads, coef);
    }
    // Corrección de sesgo calculada en el dispositivo (variante "epsilon gorro"
    // del artículo de Adam): p -= lr * sqrt(1-b2^t)/(1-b1^t) * m / (sqrt(v) + eps).
    stepCount_.add_(1.0);
    const auto bc1 = 1.0 - torch::pow(b1_, stepCount_);
    const auto bc2 = 1.0 - torch::pow(b2_, stepCount_);
    const auto scale = -lr_ * torch::sqrt(bc2) / bc1;
    at::_foreach_mul_(m_, b1_);
    at::_foreach_add_(m_, grads, 1.0f - b1_);
    at::_foreach_mul_(v_, b2_);
    at::_foreach_addcmul_(v_, grads, grads, 1.0f - b2_);
    auto denom = at::_foreach_sqrt(v_);
    at::_foreach_add_(denom, eps_);
    auto update = at::_foreach_div(m_, denom);
    at::_foreach_mul_(update, scale);
    at::_foreach_add_(params_, update);
}

torch::Tensor SacAgent::act(const torch::Tensor& obs, bool deterministic) {
    torch::NoGradGuard ng;
    return deterministic ? actor_->deterministic(obs) : actor_->sample(obs).first;
}

SacLosses SacAgent::update(const SacBatch& b) {
    SacLosses out;
    const auto alpha = logAlpha_.exp().detach();

    // Crítico: objetivo con el mínimo de las dos Q objetivo (doble Q recortada).
    torch::Tensor target;
    {
        torch::NoGradGuard ng;
        auto [nextA, nextLogp] = actor_->sample(b.nextObs);
        const auto qNext = std::get<0>(criticTarget_->forward(b.nextObs, nextA).min(0));
        target = b.rew * cfg_.rewardScale + cfg_.gamma * (1.0f - b.done) * (qNext - alpha * nextLogp);
    }
    const auto q = critic_->forward(b.obs, b.act);
    const auto criticLoss = (q - target.unsqueeze(0)).pow(2).mean() * static_cast<float>(critic_->ensemble);
    criticOpt_->zeroGrad();
    criticLoss.backward();
    criticOpt_->step();

    // Actor: maximizar Q - alpha * log pi. El crítico se congela para no calcular
    // gradientes de sus pesos que no se van a usar.
    for (auto& p : sourceParams_) p.set_requires_grad(false);
    auto [a, logp] = actor_->sample(b.obs);
    const auto qPi = std::get<0>(critic_->forward(b.obs, a).min(0));
    const auto actorLoss = (alpha * logp - qPi).mean();
    actorOpt_->zeroGrad();
    actorLoss.backward();
    actorOpt_->step();
    for (auto& p : sourceParams_) p.set_requires_grad(true);

    torch::Tensor alphaLoss = torch::zeros({1}, logAlpha_.options());
    if (cfg_.autoAlpha) {
        alphaLoss = -(logAlpha_ * (logp.detach() + cfg_.targetEntropy)).mean();
        alphaOpt_->zeroGrad();
        alphaLoss.backward();
        alphaOpt_->step();
        torch::NoGradGuard ng;
        logAlpha_.clamp_min_(std::log(cfg_.minAlpha));
    }
    softUpdate();

    out.critic = criticLoss.detach();
    out.actor = actorLoss.detach();
    out.alpha = alphaLoss.detach();
    out.alphaValue = alpha;
    out.entropy = -logp.detach().mean();
    return out;
}

SacLosses SacAgent::updateFromReplay(ReplayBuffer& replay) {
    if (graph_) {
#if WRL_CUDA_GRAPHS
        std::static_pointer_cast<at::cuda::CUDAGraph>(graph_)->replay();
#endif
        return graphLosses_;
    }
    if (cfg_.cudaGraph && device_.is_cuda() && !graphFailed_ && captureGraph(replay)) return graphLosses_;
    return update(replay.sampleGraphSafe(cfg_.batchSize));
}

bool SacAgent::captureGraph(ReplayBuffer& replay) {
#if WRL_CUDA_GRAPHS
    try {
        torch::cuda::synchronize();
        const auto stream = c10::cuda::getStreamFromPool(false, device_.index() < 0 ? 0 : device_.index());
        {
            // La captura requiere un stream distinto del por defecto. Unas cuantas
            // iteraciones previas inicializan memorias y kernels perezosos.
            c10::cuda::CUDAStreamGuard guard(stream);
            for (int i = 0; i < 3; ++i) update(replay.sampleGraphSafe(cfg_.batchSize));
            stream.synchronize();
            auto graph = std::make_shared<at::cuda::CUDAGraph>();
            graph->capture_begin();
            graphLosses_ = update(replay.sampleGraphSafe(cfg_.batchSize));
            graph->capture_end();
            graph_ = graph;
        }
        torch::cuda::synchronize();
        std::printf("Actualización de SAC capturada en un CUDA Graph.\n");
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "No se pudo capturar el CUDA Graph (%s); se sigue en modo normal.\n", e.what());
        graphFailed_ = true;
        graph_.reset();
        torch::cuda::synchronize();
        return false;
    }
#else
    (void)replay;
    graphFailed_ = true;
    return false;
#endif
}

void SacAgent::softUpdate() {
    torch::NoGradGuard ng;
    // Promedio de Polyak con operaciones "foreach": un kernel por lote de tensores.
    at::_foreach_mul_(targetParams_, 1.0f - cfg_.tau);
    at::_foreach_add_(targetParams_, sourceParams_, cfg_.tau);
}

void SacAgent::save(const std::filesystem::path& path) const {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    torch::serialize::OutputArchive archive;
    archive.write("obs_dim", torch::tensor({obsDim_, actDim_, cfg_.hidden}));
    torch::serialize::OutputArchive actorArchive, criticArchive, targetArchive;
    actor_->save(actorArchive);
    critic_->save(criticArchive);
    criticTarget_->save(targetArchive);
    archive.write("actor", actorArchive);
    archive.write("critic", criticArchive);
    archive.write("critic_target", targetArchive);
    archive.write("log_alpha", logAlpha_.detach());
    archive.save_to(path.string());
}

void SacAgent::load(const std::filesystem::path& path) {
    torch::serialize::InputArchive archive;
    archive.load_from(path.string(), device_);
    torch::Tensor meta;
    archive.read("obs_dim", meta);
    if (meta[0].item<int>() != obsDim_ || meta[2].item<int>() != cfg_.hidden)
        throw std::runtime_error("el modelo no coincide con la observación o la red configuradas");
    torch::serialize::InputArchive actorArchive, criticArchive, targetArchive;
    archive.read("actor", actorArchive);
    archive.read("critic", criticArchive);
    archive.read("critic_target", targetArchive);
    actor_->load(actorArchive);
    critic_->load(criticArchive);
    criticTarget_->load(targetArchive);
    torch::Tensor la;
    archive.read("log_alpha", la);
    torch::NoGradGuard ng;
    logAlpha_.copy_(la);
}

Actor loadActor(const std::filesystem::path& path, torch::Device device, int* obsDimOut) {
    torch::serialize::InputArchive archive;
    archive.load_from(path.string(), device);
    torch::Tensor meta;
    archive.read("obs_dim", meta);
    const int obsDim = meta[0].item<int>(), actDim = meta[1].item<int>(), hidden = meta[2].item<int>();
    Actor actor(obsDim, actDim, hidden);
    torch::serialize::InputArchive actorArchive;
    archive.read("actor", actorArchive);
    actor->load(actorArchive);
    actor->to(device);
    actor->eval();
    if (obsDimOut) *obsDimOut = obsDim;
    return actor;
}

}  // namespace wrl
