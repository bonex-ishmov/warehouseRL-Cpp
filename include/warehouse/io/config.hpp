#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "warehouse/core/math.hpp"

namespace wrl {

using Json = nlohmann::json;

struct RobotParams {
    float halfLength = 0.30f;       ///< mitad del largo (eje de avance), m
    float halfWidth = 0.25f;        ///< mitad del ancho, m
    float mass = 50.0f;             ///< masa en vacío, kg
    float trackWidth = 0.50f;       ///< separación entre ruedas, m
    float maxForwardSpeed = 1.5f;   ///< m/s
    float maxReverseSpeed = 0.3f;   ///< m/s
    float maxAngularSpeed = 2.0f;   ///< rad/s
    float maxWheelForce = 60.0f;    ///< N por rueda
    float speedGain = 300.0f;       ///< ganancia del lazo de velocidad, N/(m/s)
    float rollingResistance = 0.02f;
    float floorFriction = 0.8f;     ///< coeficiente nominal de adherencia
    float angularDamping = 1.0f;    ///< arrastre de las ruedas locas
    float bodyFriction = 0.3f;      ///< fricción del chasis contra obstáculos
    float restitution = 0.1f;
    float maxCarryMass = 30.0f;
    float commandAccel = 1.2f;         ///< rampa del controlador de motores, m/s² (0 = sin rampa)
    float commandAngularAccel = 4.0f;  ///< rad/s²

    float radius() const { return std::sqrt(halfLength * halfLength + halfWidth * halfWidth); }
};

struct EnergyParams {
    float capacityWh = 20.0f;
    float initialFraction = 1.0f;
    float basePowerW = 20.0f;
    float efficiency = 0.8f;
    float regenEfficiency = 0.0f;   ///< 0 = sin frenado regenerativo
    float chargePowerW = 250.0f;
    float reserveFraction = 0.15f;  ///< margen mínimo al planificar
    float chargeTarget = 0.95f;     ///< nivel al que se considera completa una recarga
    float unnecessaryAbove = 0.8f;  ///< recargar por encima de este nivel penaliza
};

struct LoadType {
    std::string name;
    float mass = 10.0f;
    float lateralLimit = 3.0f;       ///< aceleración lateral máxima antes de desestabilizarse, m/s²
    float longitudinalLimit = 3.0f;  ///< aceleración longitudinal máxima, m/s²
    bool fragile = false;
};

struct SensorParams {
    int lidarRays = 36;
    float lidarRange = 8.0f;
    float lidarNoise = 0.02f;     ///< desviación estándar, m
    float lidarDropout = 0.0f;    ///< probabilidad de que un rayo devuelva el alcance máximo
    float odometryNoise = 0.0f;   ///< desviación relativa de la velocidad medida
    int nearestObstacles = 3;
};

struct SimParams {
    float physicsDt = 1.0f / 60.0f;
    int subSteps = 4;
    int physicsStepsPerAction = 6;  ///< 60 Hz / 6 = control a 10 Hz
    float maxTime = 600.0f;         ///< s
    int maxCollisions = 10;
    float dockRadius = 0.4f;
    float dockSpeed = 0.1f;
    float dockAngularSpeed = 0.3f;
    float dockTime = 0.5f;
    float lookahead = 1.5f;
    float replanInterval = 2.0f;
    float offPathReplan = 1.5f;
    float gridResolution = 0.2f;
    float inflation = 0.55f;
    float clearanceCost = 2.0f;     ///< costo extra de A* cerca de obstáculos
    float nominalSpeed = 0.6f;      ///< para estimar tiempos de viaje
    float loadSmoothing = 0.1f;     ///< constante de tiempo del filtro de aceleración, s
    float collisionCooldown = 0.5f;
    float stallTimeout = 30.0f;     ///< sin acercarse al objetivo este tiempo -> se corta el episodio, s
};

struct RewardParams {
    float deliveryOnTime = 100.0f;
    float deliveryCriticalOnTime = 150.0f;
    float pickup = 15.0f;
    float chargeComplete = 15.0f;  ///< recarga completada cuando se pidió (subtarea)
    float progressPerMeter = 1.0f;
    float timePerSecond = -0.1f;
    float energyPerKJ = -0.02f;
    float jerk = -0.05f;
    float collisionGraze = -2.0f;
    float collisionLight = -25.0f;
    float collisionSevere = -100.0f;
    float loadDestabilized = -40.0f;
    float lateDelivery = -20.0f;
    float latePerSecond = -0.2f;
    float lateCap = -50.0f;           ///< la entrega tardía resta como máximo esto al premio de entrega
    float abandonedTask = -20.0f;     ///< por tarea sin entregar al terminar en fracaso
    float batteryDepleted = -50.0f;
    float unnecessaryRecharge = -2.0f;
    float criticalOverdue = -75.0f;
    float restrictedEntry = -5.0f;
    float stalled = -50.0f;    ///< episodio cortado por no avanzar (evita aprender a quedarse quieto)
    float stuckPerSecond = -1.0f;    ///< sin avanzar más de stuckGrace s: castigo denso antes del corte
    float stuckGrace = 5.0f;
    float contactPerSecond = -1.0f;  ///< empujando contra un muro, estante u obstáculo (el choque solo cuenta al empezar)
    float grazeSpeed = 0.2f;   ///< por debajo: roce
    float severeSpeed = 0.8f;  ///< por encima: grave
};

struct ProceduralParams {
    float width = 24.0f;
    float height = 24.0f;
    float aisleWidth = 2.6f;
    float shelfDepth = 1.0f;
    float shelfLength = 6.0f;
    float crossAisle = 3.0f;
    float margin = 3.0f;           ///< franja libre junto a los muros (muelles y estaciones)
    float removeShelfProb = 0.1f;
    int chargers = 2;
    int dropoffs = 4;
    int frictionZones = 0;
    float frictionMin = 0.3f;
    float frictionMax = 0.5f;
    int restrictedZones = 0;
};

struct FrictionZone {
    Rect rect;
    float mu = 0.5f;
};

struct MapSpec {
    bool procedural = true;
    ProceduralParams proc;
    float width = 24.0f, height = 24.0f;
    std::vector<Rect> shelves;
    std::vector<Rect> restricted;
    std::vector<FrictionZone> frictionZones;
    std::vector<Vec2> pickups;
    std::vector<Vec2> dropoffs;
    std::vector<Vec2> chargers;
    std::optional<Pose> robotStart;
};

struct TaskGenParams {
    int count = 1;
    int initial = 1;                ///< tareas disponibles al inicio
    float arrivalInterval = 30.0f;  ///< media del tiempo entre llegadas posteriores, s
    std::array<float, 4> priorityWeights{1.0f, 0.0f, 0.0f, 0.0f};  ///< baja, media, alta, crítica
    bool deadlines = false;
    float deadlineBase = 30.0f;     ///< holgura fija, s
    float slackMin = 1.5f;          ///< multiplicador del tiempo estimado
    float slackMax = 3.0f;
    std::vector<std::string> loadTypes;  ///< vacío = todos
};

struct ObstacleSpec {
    enum class Kind { Forklift, Worker } kind = Kind::Worker;
    std::vector<Vec2> route;   ///< vacío = ruta aleatoria por pasillos
    float speed = 0.8f;
    float pause = 1.0f;        ///< pausa en cada punto de la ruta, s
    bool stochastic = false;   ///< elige el siguiente destino al azar
};

struct ObstacleGenParams {
    int minCount = 0;
    int maxCount = 0;
    float forkliftFraction = 0.4f;
    float speedMin = 0.6f;
    float speedMax = 1.1f;
    float stochasticFraction = 0.5f;
};

struct DomainRandomization {
    bool enabled = false;
    float friction = 0.3f;     ///< ±fracción
    float mass = 0.15f;
    float motorForce = 0.1f;
    float lidarNoiseMax = 0.05f;
    float obstacleSpeed = 0.2f;
};

struct ScenarioConfig {
    std::string name = "sin_nombre";
    std::string description;
    MapSpec map;
    TaskGenParams tasks;
    ObstacleGenParams obstacles;
    std::vector<ObstacleSpec> fixedObstacles;
    RobotParams robot;
    EnergyParams energy;
    std::vector<LoadType> loadTypes;
    SensorParams sensors;
    SimParams sim;
    RewardParams reward;
    DomainRandomization randomization;
    Json acceptance = Json::object();
    Json raw;  ///< JSON completo ya fusionado (para trazas)
};

/// Carga un escenario. Admite "base": "ruta.json" (relativa al archivo) que se
/// fusiona primero y luego se sobreescribe con los campos del escenario.
ScenarioConfig loadScenario(const std::filesystem::path& path);
ScenarioConfig parseScenario(const Json& j);
Json loadJsonWithBase(const std::filesystem::path& path);

/// Comprueba consistencia y lanza std::runtime_error con un mensaje claro.
void validate(const ScenarioConfig& cfg);

}  // namespace wrl
