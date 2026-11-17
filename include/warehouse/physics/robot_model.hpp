#pragma once

#include <box2d/box2d.h>

#include "warehouse/core/math.hpp"
#include "warehouse/io/config.hpp"

namespace wrl {

/// Robot de tracción diferencial modelado como cuerpo rígido de Box2D.
///
/// El agente no aplica fuerzas: pide velocidades lineal y angular. Un lazo de
/// velocidad por rueda calcula la fuerza necesaria, saturada por la fuerza del
/// motor y por la adherencia disponible. La velocidad lateral de cada rueda se
/// cancela con un impulso limitado por el círculo de fricción: si el giro exige
/// más agarre del disponible, el robot derrapa.
class RobotModel {
public:
    struct StepPower {
        float electricalW = 0.0f;  ///< potencia consumida de la batería
        float mechanicalW = 0.0f;  ///< |F·v| sumado de ambas ruedas
        bool slipping = false;
    };

    void create(b2WorldId world, const RobotParams& params, Pose start, int robotIndex = 0);
    void setParams(const RobotParams& params);
    /// Cambia la masa transportada (0 = vacío) y actualiza masa e inercia.
    void setPayload(float mass);

    /// Aplica el control durante un paso físico. mu es la adherencia del suelo bajo el robot.
    StepPower applyControl(float linearCmd, float angularCmd, float mu, float dt, const EnergyParams& energy);

    Pose pose() const;
    Vec2 position() const;
    Vec2 velocity() const;
    Vec2 forward() const;
    float forwardSpeed() const;
    float lateralSpeed() const;
    float angularSpeed() const;
    float totalMass() const { return params_.mass + payload_; }
    float payload() const { return payload_; }
    const RobotParams& params() const { return params_; }
    b2BodyId body() const { return body_; }

private:
    void updateMassData();

    b2BodyId body_{};
    RobotParams params_;
    float payload_ = 0.0f;
};

}  // namespace wrl
