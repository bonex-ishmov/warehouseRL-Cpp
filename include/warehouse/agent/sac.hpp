#pragma once

#include <filesystem>
#include <string>

#include <torch/torch.h>

#include "warehouse/io/config.hpp"

namespace wrl {

struct SacConfig {
    int hidden = 256;
    float gamma = 0.99f;
    float tau = 0.005f;
    float actorLr = 3e-4f;
    float criticLr = 3e-4f;
    float alphaLr = 3e-4f;
    float initAlpha = 0.2f;
    bool autoAlpha = true;
    float targetEntropy = -2.0f;
    float minAlpha = 0.003f;      ///< piso de la temperatura: evita que muera la exploración
    float maxGradNorm = 5.0f;    ///< recorte de la norma del gradiente (0 = sin recorte)
    float rewardScale = 0.1f;
    int batchSize = 512;
    bool cudaGraph = true;  ///< capturar la actualización completa en un CUDA Graph
    /// LayerNorm (sin parámetros) tras cada capa oculta de los críticos: frena la
    /// divergencia de Q cuando cambia la distribución de escenarios.
    bool criticLayerNorm = false;
    /// Multiplicaciones de matrices en TF32 (núcleos tensoriales de las GPU
    /// Ampere o posteriores): ~12 % más rápido en una RTX 3050, con 10 bits de
    /// mantisa en los productos (las sumas siguen en FP32).
    bool tf32 = true;

    static SacConfig fromJson(const Json& j);
};

/// Política gaussiana con compresión tanh: a = tanh(mu + sigma * eps).
struct ActorImpl : torch::nn::Module {
    ActorImpl(int obsDim, int actDim, int hidden);
    std::pair<torch::Tensor, torch::Tensor> forward(const torch::Tensor& obs);  // media, log_std
    /// Acción muestreada y su log-probabilidad (con corrección por tanh).
    std::pair<torch::Tensor, torch::Tensor> sample(const torch::Tensor& obs);
    torch::Tensor deterministic(const torch::Tensor& obs);

    torch::nn::Linear l1{nullptr}, l2{nullptr}, mean{nullptr}, logStd{nullptr};
};
TORCH_MODULE(Actor);

/// Capa lineal de un conjunto de E redes independientes, evaluadas con un solo
/// baddbmm. Las dos Q de SAC (y sus objetivos) cuestan así una única cadena de
/// kernels en lugar de dos, lo que en una GPU pequeña reduce a la mitad las
/// llamadas de lanzamiento, que son el cuello de botella con redes de este tamaño.
struct EnsembleLinearImpl : torch::nn::Module {
    EnsembleLinearImpl(int ensemble, int in, int out);
    torch::Tensor forward(const torch::Tensor& x);  // [E, B, in] -> [E, B, out]
    torch::Tensor W, b;
};
TORCH_MODULE(EnsembleLinear);

struct CriticImpl : torch::nn::Module {
    CriticImpl(int obsDim, int actDim, int hidden, int ensemble = 2, bool layerNorm = false);
    torch::Tensor forward(const torch::Tensor& obs, const torch::Tensor& act);  // [E, B, 1]
    EnsembleLinear l1{nullptr}, l2{nullptr}, l3{nullptr};
    int ensemble;
    int hidden;
    bool layerNorm;
};
TORCH_MODULE(Critic);

struct SacBatch {
    torch::Tensor obs, act, rew, nextObs, done;
};

struct SacLosses {
    torch::Tensor critic, actor, alpha, alphaValue, entropy;  // escalares en el dispositivo
};

/// Adam implementado con operaciones "foreach" de ATen: actualiza todos los
/// parámetros con unos pocos kernels de varios tensores, en lugar de los ~7
/// kernels por tensor de torch::optim::Adam. El contador de pasos vive en el
/// dispositivo, así que el paso es capturable en un CUDA Graph.
class ForeachAdam {
public:
    ForeachAdam(std::vector<torch::Tensor> params, float lr, float maxGradNorm = 0.0f, float beta1 = 0.9f, float beta2 = 0.999f,
                float eps = 1e-8f);
    void zeroGrad();
    void step();

private:
    std::vector<torch::Tensor> params_, m_, v_;
    torch::Tensor stepCount_;
    float lr_, maxNorm_, b1_, b2_, eps_;
};

class ReplayBuffer;

/// Soft Actor-Critic (Haarnoja et al., 2018) con ajuste automático de la entropía.
class SacAgent {
public:
    SacAgent(int obsDim, int actDim, const SacConfig& cfg, torch::Device device);

    /// Acciones para un lote de observaciones ya en el dispositivo.
    torch::Tensor act(const torch::Tensor& obs, bool deterministic);
    /// Una actualización. No sincroniza con la CPU (los escalares quedan en la GPU).
    SacLosses update(const SacBatch& batch);
    /// Una actualización con un lote muestreado de la memoria. En CUDA, la primera
    /// llamada captura muestreo + actualización completa en un CUDA Graph y las
    /// siguientes solo lo reproducen (un lanzamiento en lugar de cientos).
    SacLosses updateFromReplay(ReplayBuffer& replay);
    bool usingCudaGraph() const { return graph_ != nullptr; }

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);

    Actor& actor() { return actor_; }
    torch::Device device() const { return device_; }
    int obsDim() const { return obsDim_; }

private:
    void softUpdate();
    bool captureGraph(ReplayBuffer& replay);

    int obsDim_, actDim_;
    SacConfig cfg_;
    torch::Device device_;
    Actor actor_;
    Critic critic_, criticTarget_;
    torch::Tensor logAlpha_;
    std::unique_ptr<ForeachAdam> actorOpt_, criticOpt_, alphaOpt_;
    std::vector<torch::Tensor> targetParams_, sourceParams_;
    std::shared_ptr<void> graph_;  // at::cuda::CUDAGraph (oculto para no exponer cabeceras CUDA)
    bool graphFailed_ = false;
    SacLosses graphLosses_;
};

/// Carga solo el actor desde un archivo guardado por SacAgent::save (para evaluar).
Actor loadActor(const std::filesystem::path& path, torch::Device device, int* obsDimOut = nullptr);

/// "auto" elige CUDA si está disponible.
torch::Device resolveDevice(const std::string& name);

/// Pide que las esperas a la GPU duerman el hilo en vez de girar en un bucle
/// activo (por defecto CUDA ocupa un núcleo entero mientras espera). Debe
/// llamarse antes de usar CUDA. Devuelve false si no se pudo aplicar.
bool preferBlockingCudaSync();

}  // namespace wrl
