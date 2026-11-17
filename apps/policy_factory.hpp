#pragma once

// Fábrica de políticas para las aplicaciones: une las del núcleo con la SAC,
// que solo existe si se compiló el agente (LibTorch).

#include <memory>
#include <stdexcept>

#include "warehouse/session/session.hpp"
#ifdef WRL_HAS_TORCH
#include "warehouse/agent/sac_policy.hpp"
#endif

namespace wrl {

inline std::unique_ptr<Policy> makePolicy(const AgentSpec& spec) {
    if (spec.kind == "sac") {
#ifdef WRL_HAS_TORCH
        // Inferencia de un solo estado por paso: en CPU es más rápida que en la GPU.
        return makeSacPolicy(spec.model, "cpu");
#else
        throw std::runtime_error("este binario se compiló sin el agente SAC (WRL_BUILD_AGENT=OFF)");
#endif
    }
    return makeBuiltinPolicy(spec.kind);
}

inline bool hasSacSupport() {
#ifdef WRL_HAS_TORCH
    return true;
#else
    return false;
#endif
}

}  // namespace wrl
