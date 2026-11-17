#include "warehouse/agent/replay_buffer.hpp"

namespace wrl {

ReplayBuffer::ReplayBuffer(int capacity, int obsDim, int actDim, torch::Device device) : capacity_(capacity) {
    const auto o = torch::TensorOptions().dtype(torch::kFloat32).device(device);
    obs_ = torch::zeros({capacity, obsDim}, o);
    next_ = torch::zeros({capacity, obsDim}, o);
    act_ = torch::zeros({capacity, actDim}, o);
    rew_ = torch::zeros({capacity, 1}, o);
    done_ = torch::zeros({capacity, 1}, o);
    sizeT_ = torch::zeros({1}, o);
}

void ReplayBuffer::add(const torch::Tensor& obs, const torch::Tensor& act, const torch::Tensor& rew,
                       const torch::Tensor& nextObs, const torch::Tensor& done) {
    const int n = static_cast<int>(obs.size(0));
    torch::NoGradGuard ng;
    int written = 0;
    while (written < n) {
        const int chunk = std::min(n - written, capacity_ - pos_);
        auto dst = torch::indexing::Slice(pos_, pos_ + chunk);
        auto src = torch::indexing::Slice(written, written + chunk);
        obs_.index_put_({dst}, obs.index({src}));
        act_.index_put_({dst}, act.index({src}));
        rew_.index_put_({dst}, rew.index({src}).view({-1, 1}));
        next_.index_put_({dst}, nextObs.index({src}));
        done_.index_put_({dst}, done.index({src}).view({-1, 1}));
        pos_ = (pos_ + chunk) % capacity_;
        written += chunk;
    }
    size_ = std::min(capacity_, size_ + n);
    sizeT_.fill_(static_cast<float>(size_));
}

SacBatch ReplayBuffer::sampleGraphSafe(int batch) {
    const auto u = torch::rand({batch}, obs_.options());
    const auto idx = (u * sizeT_).to(torch::kLong).clamp_max(capacity_ - 1);
    return {obs_.index_select(0, idx), act_.index_select(0, idx), rew_.index_select(0, idx), next_.index_select(0, idx),
            done_.index_select(0, idx)};
}

SacBatch ReplayBuffer::sample(int batch) {
    const auto idx = torch::randint(size_, {batch}, torch::TensorOptions().dtype(torch::kLong).device(obs_.device()));
    return {obs_.index_select(0, idx), act_.index_select(0, idx), rew_.index_select(0, idx), next_.index_select(0, idx),
            done_.index_select(0, idx)};
}

size_t ReplayBuffer::bytes() const {
    size_t b = 0;
    for (const auto* t : {&obs_, &act_, &rew_, &next_, &done_}) b += static_cast<size_t>(t->numel()) * t->element_size();
    return b;
}

}  // namespace wrl
