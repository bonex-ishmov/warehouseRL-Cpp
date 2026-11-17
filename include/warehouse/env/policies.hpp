#pragma once

#include <memory>
#include <string>

#include "warehouse/env/warehouse_env.hpp"
#include "warehouse/planning/dwa.hpp"

namespace wrl {

/// Política de movimiento: recibe el entorno (solo lectura) y devuelve una acción.
class Policy {
public:
    virtual ~Policy() = default;
    virtual std::string name() const = 0;
    virtual void reset(uint64_t /*seed*/) {}
    virtual Action act(const WarehouseEnv& env) = 0;
};

class RandomPolicy final : public Policy {
public:
    std::string name() const override { return "aleatoria"; }
    void reset(uint64_t seed) override { rng_.seed(seed); }
    Action act(const WarehouseEnv& env) override;

private:
    Rng rng_{7};
};

/// Línea base clásica: ruta global A* (del entorno) + controlador local DWA.
class DwaPolicy final : public Policy {
public:
    std::string name() const override { return "astar_dwa"; }
    Action act(const WarehouseEnv& env) override;

private:
    std::unique_ptr<DWAController> dwa_;
};

/// Conducción por teclado: la interfaz fija la acción y el entorno la aplica.
class ManualPolicy final : public Policy {
public:
    std::string name() const override { return "manual"; }
    void set(Action a) { action_ = a; }
    Action act(const WarehouseEnv&) override { return action_; }

private:
    Action action_;
};

/// "random", "dwa" o "manual". La política SAC se crea en wrl_agent (makeSacPolicy).
std::unique_ptr<Policy> makeBuiltinPolicy(const std::string& name);

}  // namespace wrl
