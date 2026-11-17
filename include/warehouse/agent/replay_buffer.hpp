#pragma once

#include <torch/torch.h>

#include "warehouse/agent/sac.hpp"

namespace wrl {

/// Memoria de repetición circular residente en el dispositivo (GPU si hay).
/// Las transiciones de todos los entornos entran en bloque, y el muestreo usa
/// índices aleatorios generados en el propio dispositivo: no hay copias
/// CPU→GPU por muestra, solo una por paso vectorizado.
class ReplayBuffer {
public:
    ReplayBuffer(int capacity, int obsDim, int actDim, torch::Device device);

    /// Tensores [N, ...] ya en el dispositivo.
    void add(const torch::Tensor& obs, const torch::Tensor& act, const torch::Tensor& rew, const torch::Tensor& nextObs,
             const torch::Tensor& done);
    SacBatch sample(int batch);
    /// Igual que sample, pero el tamaño actual se lee de un tensor del
    /// dispositivo: se puede capturar en un CUDA Graph y seguir siendo correcto
    /// mientras la memoria crece.
    SacBatch sampleGraphSafe(int batch);

    int size() const { return size_; }
    int capacity() const { return capacity_; }
    /// Memoria ocupada en bytes.
    size_t bytes() const;

private:
    int capacity_, size_ = 0, pos_ = 0;
    torch::Tensor obs_, act_, rew_, next_, done_;
    torch::Tensor sizeT_;
};

}  // namespace wrl
