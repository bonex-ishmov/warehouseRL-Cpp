#pragma once

#include <filesystem>
#include <memory>

#include "warehouse/env/policies.hpp"

namespace wrl {

/// Política SAC entrenada (actor determinista, sin exploración) para evaluar o visualizar.
std::unique_ptr<Policy> makeSacPolicy(const std::filesystem::path& model, const std::string& device = "cpu");

}  // namespace wrl
