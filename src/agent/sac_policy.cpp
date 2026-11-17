#include "warehouse/agent/sac_policy.hpp"

#include <cstring>
#include <stdexcept>

#include "warehouse/agent/sac.hpp"

namespace wrl {
namespace {

class SacPolicy final : public Policy {
public:
    SacPolicy(const std::filesystem::path& model, torch::Device device) : device_(device), actor_(loadActor(model, device, &obsDim_)) {
        input_ = torch::zeros({1, obsDim_});
    }

    std::string name() const override { return "sac"; }

    Action act(const WarehouseEnv& env) override {
        const auto& obs = env.observation();
        if (static_cast<int>(obs.size()) != obsDim_)
            throw std::runtime_error("el modelo espera " + std::to_string(obsDim_) + " observaciones y el escenario produce " +
                                     std::to_string(obs.size()));
        torch::NoGradGuard ng;
        std::memcpy(input_.data_ptr<float>(), obs.data(), obs.size() * sizeof(float));
        const auto a = actor_->deterministic(input_.to(device_)).to(torch::kCPU);
        return {a[0][0].item<float>(), a[0][1].item<float>()};
    }

private:
    torch::Device device_;
    int obsDim_ = 0;
    Actor actor_;
    torch::Tensor input_;
};

}  // namespace

std::unique_ptr<Policy> makeSacPolicy(const std::filesystem::path& model, const std::string& device) {
    // Cada política se usa desde un único hilo; la inferencia de una red tan
    // pequeña es más rápida sin el paralelismo interno de Torch.
    torch::set_num_threads(1);
    return std::make_unique<SacPolicy>(model, resolveDevice(device));
}

}  // namespace wrl
