#pragma once

#include "warehouse/io/config.hpp"

namespace wrl::test {

/// Escenario explícito pequeño: una estantería en el centro, puntos fijos.
ScenarioConfig smallScenario(int tasks = 1);
/// Carga un escenario del repositorio por nombre de archivo.
ScenarioConfig repoScenario(const std::string& file);

}  // namespace wrl::test
