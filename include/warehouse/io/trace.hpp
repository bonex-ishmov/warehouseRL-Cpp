#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "warehouse/env/warehouse_env.hpp"

namespace wrl {

/// Traza reproducible: escenario + semilla + acciones. Como la simulación es
/// determinista, reproducirla consiste en volver a simular con esas acciones.
struct Trace {
    Json scenario;
    uint64_t seed = 0;
    std::string policy;
    std::vector<Action> actions;

    void save(const std::filesystem::path& path) const;
    static Trace load(const std::filesystem::path& path);
};

/// Estado por paso en CSV legible, para análisis y gráficas.
class StateRecorder {
public:
    void record(const WarehouseEnv& env, const StepResult& r);
    void save(const std::filesystem::path& csvPath) const;
    void clear() { rows_.clear(); }

private:
    std::vector<std::string> rows_;
};

}  // namespace wrl
