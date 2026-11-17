#pragma once

#include <vector>

#include "warehouse/core/math.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/planning/path.hpp"
#include "warehouse/sensors/sensor_suite.hpp"

namespace wrl {

/// Dynamic Window Approach: controlador local clásico de la línea base.
/// Muestrea velocidades alcanzables con la aceleración disponible, simula
/// cada trayectoria con un modelo de uniciclo y puntúa rumbo, holgura y
/// velocidad. Los obstáculos móviles se proyectan con velocidad constante.
class DWAController {
public:
    struct Params {
        float horizon = 2.0f;
        float simDt = 0.2f;
        int linearSamples = 9;
        int angularSamples = 17;
        float window = 0.4f;        ///< tiempo para el que se calcula la ventana dinámica, s
        float headingWeight = 1.0f;
        float clearanceWeight = 0.8f;
        float speedWeight = 0.5f;
        float progressWeight = 1.5f;
        float pathWeight = 1.2f;    ///< penaliza alejarse de la ruta global
        float stuckTime = 4.0f;     ///< sin avanzar 0.3 m en este tiempo -> recuperación, s
        float recoveryTime = 1.5f;
        float safety = 0.08f;       ///< margen mínimo, m
        float goalSlowdown = 1.6f;  ///< distancia al objetivo a partir de la que se frena, m
    };

    struct Input {
        Pose pose;
        float v = 0.0f;
        float w = 0.0f;
        Vec2 carrot;
        Vec2 goal;
        float dockRadius = 0.4f;
        float totalMass = 50.0f;
        float friction = 0.8f;
        const std::vector<float>* lidar = nullptr;
        const SensorSuite* sensors = nullptr;
        const std::vector<PerceivedObstacle>* obstacles = nullptr;
        const Path* path = nullptr;
        float maxAccel = 1e9f;  ///< límite de la carga transportada, m/s²
        float dt = 0.1f;        ///< periodo de control
    };

    struct Command {
        float v = 0.0f;
        float w = 0.0f;
    };

    explicit DWAController(const RobotParams& robot) : DWAController(robot, Params{}) {}
    DWAController(const RobotParams& robot, Params params);
    Command compute(const Input& in);

private:
    Command search(const Input& in, Vec2 target, bool aimAtGoal, float goalDist);

    RobotParams robot_;
    Params p_;
    std::vector<Vec2> points_;  // nube de puntos del LiDAR en el mundo
    float stuck_ = 0.0f;
    float recovery_ = 0.0f;
    int turnDir_ = 0;     ///< giro en el sitio en curso (+1/-1), con sentido fijo
    Vec2 anchor_;         ///< posición de referencia para detectar atascos
};

}  // namespace wrl
