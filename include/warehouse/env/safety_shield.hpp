#pragma once

#include <vector>

#include "warehouse/core/math.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/physics/dynamic_obstacles.hpp"
#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

/// Capa de seguridad fija entre la política y los motores, como el escáner de seguridad
/// de un AGV (speed and separation monitoring, ISO 3691-4): limita la consigna para que
/// ningún punto del robot se acerque a un muro, estantería u obstáculo más rápido de lo
/// que permite frenar antes de tocarlo. No decide la ruta ni frena más fuerte que la
/// rampa de los motores: solo recorta la velocidad de la consigna.
class SafetyShield {
public:
    struct Result {
        float v = 0.0f, w = 0.0f;  ///< consigna resultante, m/s y rad/s
        bool limited = false;      ///< la capa recortó la consigna
    };

    /// v, w: consigna pedida por la política. pose: estado actual del robot.
    /// controlDt: cuánto se mantiene la consigna antes de volver a decidir (s).
    Result filter(const ShieldParams& p, const RobotParams& rp, const WarehouseMap& map,
                  const std::vector<DynamicObstacle>& obstacles, Pose pose, float v, float w,
                  float controlDt) const;

    /// Velocidad máxima de acercamiento con hueco room (m) y decisión cada controlDt.
    static float allowedClosing(float room, float decel, float controlDt);

private:
    /// Restricción lineal sobre la consigna: alpha·v + beta·w <= bound.
    struct Constraint {
        float alpha, beta, bound;
    };
    mutable std::vector<Constraint> cons_;
};

}  // namespace wrl
