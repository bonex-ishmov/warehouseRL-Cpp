#include "warehouse/core/rng.hpp"

namespace wrl {

// Se implementan las distribuciones a mano: las de <random> no garantizan la
// misma secuencia entre implementaciones de la librería estándar.
float Rng::uniform(float lo, float hi) {
    const double u = static_cast<double>(engine_() >> 11) * (1.0 / 9007199254740992.0);
    return lo + static_cast<float>(u) * (hi - lo);
}

int Rng::uniformInt(int lo, int hiInclusive) {
    const uint64_t span = static_cast<uint64_t>(hiInclusive - lo) + 1;
    return lo + static_cast<int>(engine_() % span);
}

float Rng::normal(float mean, float stddev) {
    // Box-Muller
    float u1 = uniform();
    while (u1 <= 1e-12f) u1 = uniform();
    const float u2 = uniform();
    const float z = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * 3.14159265f * u2);
    return mean + stddev * z;
}

uint64_t mixSeed(uint64_t base, uint64_t index) {
    uint64_t z = base + 0x9E3779B97F4A7C15ull * (index + 1);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

}  // namespace wrl
