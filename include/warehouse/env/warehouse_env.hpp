#pragma once

#include <memory>
#include <string>
#include <vector>

#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"
#include "warehouse/physics/dynamic_obstacles.hpp"
#include "warehouse/physics/physics_world.hpp"
#include "warehouse/physics/robot_model.hpp"
#include "warehouse/planning/astar.hpp"
#include "warehouse/sensors/sensor_suite.hpp"
#include "warehouse/sim/task_manager.hpp"
#include "warehouse/sim/task_selector.hpp"
#include "warehouse/world/occupancy_grid.hpp"

namespace wrl {

/// Acción continua normalizada en [-1, 1].
struct Action {
    float linear = 0.0f;
    float angular = 0.0f;
};

enum class EndReason : uint8_t { None, AllDelivered, BatteryDepleted, TimeLimit, WorkerCollision, CollisionLimit, Stalled };
const char* toString(EndReason r);

enum class Severity : uint8_t { Graze, Light, Severe };

/// Eventos ocurridos durante un paso de control.
struct StepEvents {
    int pickups = 0;
    int deliveriesOnTime = 0;
    int deliveriesLate = 0;
    int criticalOnTime = 0;
    int collisions[3] = {0, 0, 0};
    int workerHits[3] = {0, 0, 0};  ///< de los anteriores, los que fueron con operarios
    float collisionPenalty = 0.0f;  ///< castigo total de los choques del paso (según gravedad, operario y culpa)
    int workerSevere = 0;
    int loadDrops = 0;
    int criticalOverdue = 0;
    int restrictedEntries = 0;
    int rechargesStarted = 0;
    int unnecessaryRecharges = 0;
    bool batteryDepleted = false;
    bool replanned = false;
};

struct StepResult {
    float reward = 0.0f;
    bool terminated = false;
    bool truncated = false;
    EndReason reason = EndReason::None;
    StepEvents events;

    bool done() const { return terminated || truncated; }
};

/// Métricas acumuladas de un episodio (sección 10.2 de la propuesta).
struct EpisodeMetrics {
    int tasksTotal = 0;
    int delivered = 0;
    int onTime = 0;
    int late = 0;
    int criticalOverdue = 0;
    int collisions[3] = {0, 0, 0};
    int workerSevere = 0;
    int loadIncidents = 0;
    int recharges = 0;
    int unnecessaryRecharges = 0;
    int restrictedEntries = 0;
    bool batteryDepleted = false;
    double energyWh = 0.0;
    double distance = 0.0;
    double legActual = 0.0;    ///< distancia recorrida en tramos completados
    double legShortest = 0.0;  ///< longitud más corta de esos mismos tramos
    double sumDeliveryTime = 0.0;
    double sumDelay = 0.0;
    double totalReturn = 0.0;
    double simTime = 0.0;
    double minClearance = 1e9;
    double sumJerk = 0.0;
    double sumAbsAccel = 0.0;
    int steps = 0;
    int slipSteps = 0;
    double unsafeWorkerTime = 0.0;    ///< s yendo > 0.1 m/s más rápido de lo que permite frenar ante un operario
    double unsafeForkliftTime = 0.0;
    EndReason reason = EndReason::None;

    double completionRate() const { return tasksTotal ? static_cast<double>(delivered) / tasksTotal : 0.0; }
    double onTimeRate() const { return tasksTotal ? static_cast<double>(onTime) / tasksTotal : 0.0; }
    double pathRatio() const { return legShortest > 0 ? legActual / legShortest : 0.0; }
    int harmfulCollisions() const { return collisions[1] + collisions[2]; }
};

/// Entorno de aprendizaje: almacén 2D continuo con física de cuerpo rígido.
///
/// Un paso de control (10 Hz por defecto) mantiene la acción durante varios
/// pasos físicos. La jerarquía de la propuesta queda así: el selector de
/// tareas (nivel alto) y el A* (nivel medio) viven dentro del entorno; la
/// política externa solo controla el movimiento (nivel bajo).
class WarehouseEnv {
public:
    explicit WarehouseEnv(ScenarioConfig cfg);
    ~WarehouseEnv();
    WarehouseEnv(const WarehouseEnv&) = delete;
    WarehouseEnv& operator=(const WarehouseEnv&) = delete;

    void reset(uint64_t seed);
    /// Reinicia con otro escenario (p. ej. uno generado). Debe dar la misma observación.
    void reset(uint64_t seed, ScenarioConfig cfg);
    StepResult step(const Action& action);

    static constexpr int kActionSize = 2;
    int observationSize() const { return obsSize_; }
    static int observationSize(const SensorParams& s) { return s.lidarRays + 26 + kPerObstacle * s.nearestObstacles; }
    static constexpr int kPerObstacle = 8;
    /// Formato de los modelos anteriores al v11: 5 valores por obstáculo (sin tipo,
    /// distancia entre superficies ni acercamiento) y obstáculos ordenados por centro.
    static int legacyObservationSize(const SensorParams& s) { return s.lidarRays + 26 + 5 * s.nearestObstacles; }
    /// Convierte una observación actual (observationSize) a ese formato (legacyObservationSize).
    static void toLegacyObservation(const SensorParams& s, const float* in, float* out);
    const std::vector<float>& observation() const { return obs_; }

    /// Guarda las acciones del episodio para reproducirlo después.
    void setRecordActions(bool on) { recordActions_ = on; }
    const std::vector<Action>& recordedActions() const { return actions_; }

    // --- Vistas de solo lectura (visor, línea base, pruebas) ---
    const ScenarioConfig& config() const { return cfg_; }
    const ScenarioConfig& baseConfig() const { return base_; }
    const WarehouseMap& map() const { return map_; }
    const OccupancyGrid& grid() const { return grid_; }
    const PhysicsWorld& physics() const { return *world_; }
    const RobotModel& robot() const { return robot_; }
    const std::vector<DynamicObstacle>& obstacles() const { return obstacles_.obstacles(); }
    const TaskManager& tasks() const { return tasks_; }
    const Path& path() const { return path_; }
    const Decision& decision() const { return decision_; }
    Vec2 carrot() const { return carrot_; }
    const std::vector<float>& lidar() const { return lidar_; }
    const std::vector<PerceivedObstacle>& perceived() const { return perceived_; }
    const SensorSuite& sensors() const { return sensors_; }
    float measuredSpeed() const { return measuredV_; }
    float measuredAngular() const { return measuredW_; }
    float batteryWh() const { return batteryWh_; }
    float batteryFraction() const { return batteryWh_ / cfg_.energy.capacityWh; }
    int carryingTask() const { return carrying_; }
    float dockProgress() const { return dockTimer_ / cfg_.sim.dockTime; }
    bool charging() const { return charging_; }
    /// El robot está en contacto físico con algo en este momento.
    bool touching() const;
    double time() const { return time_; }
    float controlDt() const { return cfg_.sim.physicsDt * static_cast<float>(cfg_.sim.physicsStepsPerAction); }
    float frictionUnderRobot() const;
    /// Aceleración que siente la carga respecto a su límite (0 sin carga, >= 1 se cae).
    float loadStress() const;
    /// Exceso de velocidad hacia los obstáculos percibidos respecto a la que permite
    /// frenar a tiempo (m/s), por separado para operarios y montacargas.
    void unsafeSpeed(float& worker, float& forklift) const;
    const EpisodeMetrics& metrics() const { return metrics_; }
    uint64_t seed() const { return seed_; }
    Action lastAction() const { return lastAction_; }
    bool done() const { return done_; }

private:
    void buildEpisode();
    void applyRandomization();
    void physicsStep(float dt, float vCmd, float wCmd, StepEvents& ev, double& energyJ);
    void handleContacts(StepEvents& ev, Vec2 vBefore);
    void updateDocking(float dt, StepEvents& ev, float& reward);
    void decide(bool force);
    void replan();
    bool stalled();
    void updateCarrot();
    void startLeg();
    void finishLeg();
    float potential() const;
    void buildObservation();

    ScenarioConfig base_;  ///< configuración sin aleatorizar
    ScenarioConfig cfg_;   ///< configuración del episodio (con aleatorización de dominio)
    Rng rng_;
    uint64_t seed_ = 0;

    WarehouseMap map_;
    OccupancyGrid grid_;
    AStarPlanner planner_;
    std::unique_ptr<PhysicsWorld> world_;
    RobotModel robot_;
    DynamicObstacleSystem obstacles_;
    SensorSuite sensors_;
    TaskManager tasks_;
    TaskSelector selector_;
    float frictionScale_ = 1.0f;
    float obstacleSpeedScale_ = 1.0f;
    int pathVersion_ = 0;
    float bestRemaining_ = 0.0f;  ///< menor distancia restante al objetivo actual
    float stallTimer_ = 0.0f;

    Decision decision_;
    Path path_;
    Vec2 carrot_;
    float replanTimer_ = 0.0f;
    int carrying_ = -1;
    float batteryWh_ = 0.0f;
    float dockTimer_ = 0.0f;
    bool charging_ = false;
    bool touching_ = false;
    bool inRestricted_ = false;
    Vec2 accelFiltered_;
    Vec2 prevVel_;
    std::vector<std::pair<uint32_t, double>> contactCooldown_;
    double time_ = 0.0;
    bool done_ = false;

    double legStartOdometer_ = 0.0;
    float legShortest_ = 0.0f;
    bool legActive_ = false;

    std::vector<float> lidar_;
    std::vector<PerceivedObstacle> perceived_;
    float measuredV_ = 0.0f, measuredW_ = 0.0f;
    float rampV_ = 0.0f, rampW_ = 0.0f;  ///< consigna tras la rampa del controlador de motores
    Action lastAction_;
    int obsSize_ = 0;
    std::vector<float> obs_;
    EpisodeMetrics metrics_;
    bool recordActions_ = false;
    std::vector<Action> actions_;
};

}  // namespace wrl
