#pragma once

#include <cstdint>
#include <random>

namespace wrl {

/// Generador reproducible. Cada entorno tiene el suyo: nunca se comparte entre hilos.
class Rng {
public:
    explicit Rng(uint64_t seed = 0) : engine_(seed) {}

    void seed(uint64_t s) { engine_.seed(s); }
    float uniform(float lo = 0.0f, float hi = 1.0f);
    int uniformInt(int lo, int hiInclusive);
    float normal(float mean = 0.0f, float stddev = 1.0f);
    bool bernoulli(float p) { return uniform() < p; }
    /// Deriva una semilla independiente para un subsistema.
    uint64_t split() { return engine_() ^ 0x9E3779B97F4A7C15ull; }

    std::mt19937_64& engine() { return engine_; }

private:
    std::mt19937_64 engine_;
};

/// Mezcla de una semilla base con un índice (SplitMix64).
uint64_t mixSeed(uint64_t base, uint64_t index);

}  // namespace wrl
