#include "warehouse/io/config.hpp"

#include <fstream>
#include <stdexcept>

namespace wrl {
namespace {

template <typename T>
void read(const Json& j, const char* key, T& out) {
    if (auto it = j.find(key); it != j.end() && !it->is_null()) out = it->get<T>();
}

Vec2 toVec(const Json& j) {
    if (j.is_array() && j.size() == 2) return {j[0].get<float>(), j[1].get<float>()};
    if (j.is_object()) return {j.at("x").get<float>(), j.at("y").get<float>()};
    throw std::runtime_error("se esperaba un punto [x, y]: " + j.dump());
}

Rect toRect(const Json& j) {
    return {j.at("x").get<float>(), j.at("y").get<float>(), j.at("w").get<float>(), j.at("h").get<float>()};
}

std::vector<Vec2> toVecList(const Json& j) {
    std::vector<Vec2> out;
    for (const auto& p : j) out.push_back(toVec(p));
    return out;
}

void parseRobot(const Json& j, RobotParams& r) {
    read(j, "halfLength", r.halfLength);
    read(j, "halfWidth", r.halfWidth);
    read(j, "mass", r.mass);
    read(j, "trackWidth", r.trackWidth);
    read(j, "maxForwardSpeed", r.maxForwardSpeed);
    read(j, "maxReverseSpeed", r.maxReverseSpeed);
    read(j, "maxAngularSpeed", r.maxAngularSpeed);
    read(j, "maxWheelForce", r.maxWheelForce);
    read(j, "speedGain", r.speedGain);
    read(j, "rollingResistance", r.rollingResistance);
    read(j, "floorFriction", r.floorFriction);
    read(j, "angularDamping", r.angularDamping);
    read(j, "bodyFriction", r.bodyFriction);
    read(j, "restitution", r.restitution);
    read(j, "maxCarryMass", r.maxCarryMass);
    read(j, "commandAccel", r.commandAccel);
    read(j, "commandAngularAccel", r.commandAngularAccel);
}

void parseEnergy(const Json& j, EnergyParams& e) {
    read(j, "capacityWh", e.capacityWh);
    read(j, "initialFraction", e.initialFraction);
    read(j, "basePowerW", e.basePowerW);
    read(j, "efficiency", e.efficiency);
    read(j, "regenEfficiency", e.regenEfficiency);
    read(j, "chargePowerW", e.chargePowerW);
    read(j, "reserveFraction", e.reserveFraction);
    read(j, "chargeTarget", e.chargeTarget);
    read(j, "unnecessaryAbove", e.unnecessaryAbove);
}

void parseSensors(const Json& j, SensorParams& s) {
    read(j, "lidarRays", s.lidarRays);
    read(j, "lidarRange", s.lidarRange);
    read(j, "lidarNoise", s.lidarNoise);
    read(j, "lidarDropout", s.lidarDropout);
    read(j, "odometryNoise", s.odometryNoise);
    read(j, "nearestObstacles", s.nearestObstacles);
}

void parseSim(const Json& j, SimParams& s) {
    read(j, "physicsDt", s.physicsDt);
    read(j, "subSteps", s.subSteps);
    read(j, "physicsStepsPerAction", s.physicsStepsPerAction);
    read(j, "maxTime", s.maxTime);
    read(j, "maxCollisions", s.maxCollisions);
    read(j, "dockRadius", s.dockRadius);
    read(j, "dockSpeed", s.dockSpeed);
    read(j, "dockAngularSpeed", s.dockAngularSpeed);
    read(j, "dockTime", s.dockTime);
    read(j, "lookahead", s.lookahead);
    read(j, "replanInterval", s.replanInterval);
    read(j, "offPathReplan", s.offPathReplan);
    read(j, "gridResolution", s.gridResolution);
    read(j, "inflation", s.inflation);
    read(j, "clearanceCost", s.clearanceCost);
    read(j, "nominalSpeed", s.nominalSpeed);
    read(j, "loadSmoothing", s.loadSmoothing);
    read(j, "collisionCooldown", s.collisionCooldown);
    read(j, "stallTimeout", s.stallTimeout);
}

void parseReward(const Json& j, RewardParams& r) {
    read(j, "deliveryOnTime", r.deliveryOnTime);
    read(j, "deliveryCriticalOnTime", r.deliveryCriticalOnTime);
    read(j, "pickup", r.pickup);
    read(j, "chargeComplete", r.chargeComplete);
    read(j, "progressPerMeter", r.progressPerMeter);
    read(j, "timePerSecond", r.timePerSecond);
    read(j, "energyPerKJ", r.energyPerKJ);
    read(j, "jerk", r.jerk);
    read(j, "collisionGraze", r.collisionGraze);
    read(j, "collisionLight", r.collisionLight);
    read(j, "collisionSevere", r.collisionSevere);
    read(j, "loadDestabilized", r.loadDestabilized);
    read(j, "lateDelivery", r.lateDelivery);
    read(j, "latePerSecond", r.latePerSecond);
    read(j, "lateCap", r.lateCap);
    read(j, "abandonedTask", r.abandonedTask);
    read(j, "batteryDepleted", r.batteryDepleted);
    read(j, "unnecessaryRecharge", r.unnecessaryRecharge);
    read(j, "criticalOverdue", r.criticalOverdue);
    read(j, "restrictedEntry", r.restrictedEntry);
    read(j, "stalled", r.stalled);
    read(j, "stuckPerSecond", r.stuckPerSecond);
    read(j, "stuckGrace", r.stuckGrace);
    read(j, "contactPerSecond", r.contactPerSecond);
    read(j, "unsafeWorkerPerSecond", r.unsafeWorkerPerSecond);
    read(j, "unsafeForkliftPerSecond", r.unsafeForkliftPerSecond);
    read(j, "unsafeStaticPerSecond", r.unsafeStaticPerSecond);
    read(j, "safetyMarginStatic", r.safetyMarginStatic);
    read(j, "staticCollisionScale", r.staticCollisionScale);
    read(j, "safetyDecel", r.safetyDecel);
    read(j, "safetyMarginWorker", r.safetyMarginWorker);
    read(j, "safetyMarginForklift", r.safetyMarginForklift);
    read(j, "safetyLookahead", r.safetyLookahead);
    read(j, "workerCollisionScale", r.workerCollisionScale);
    read(j, "collisionNoFaultScale", r.collisionNoFaultScale);
    read(j, "grazeSpeed", r.grazeSpeed);
    read(j, "severeSpeed", r.severeSpeed);
}

void parseMap(const Json& j, MapSpec& m) {
    if (auto p = j.find("procedural"); p != j.end()) {
        m.procedural = true;
        const Json& pj = *p;
        auto& pp = m.proc;
        read(pj, "width", pp.width);
        read(pj, "height", pp.height);
        read(pj, "aisleWidth", pp.aisleWidth);
        read(pj, "shelfDepth", pp.shelfDepth);
        read(pj, "shelfLength", pp.shelfLength);
        read(pj, "crossAisle", pp.crossAisle);
        read(pj, "margin", pp.margin);
        read(pj, "removeShelfProb", pp.removeShelfProb);
        read(pj, "chargers", pp.chargers);
        read(pj, "dropoffs", pp.dropoffs);
        read(pj, "frictionZones", pp.frictionZones);
        read(pj, "frictionMin", pp.frictionMin);
        read(pj, "frictionMax", pp.frictionMax);
        read(pj, "restrictedZones", pp.restrictedZones);
        m.width = pp.width;
        m.height = pp.height;
        return;
    }
    m.procedural = false;
    read(j, "width", m.width);
    read(j, "height", m.height);
    m.shelves.clear();
    m.restricted.clear();
    m.frictionZones.clear();
    if (j.contains("shelves")) for (const auto& r : j["shelves"]) m.shelves.push_back(toRect(r));
    if (j.contains("restricted")) for (const auto& r : j["restricted"]) m.restricted.push_back(toRect(r));
    if (j.contains("frictionZones"))
        for (const auto& z : j["frictionZones"]) m.frictionZones.push_back({toRect(z), z.value("mu", 0.5f)});
    if (j.contains("pickups")) m.pickups = toVecList(j["pickups"]);
    if (j.contains("dropoffs")) m.dropoffs = toVecList(j["dropoffs"]);
    if (j.contains("chargers")) m.chargers = toVecList(j["chargers"]);
    if (j.contains("robotStart")) {
        const auto& s = j["robotStart"];
        m.robotStart = Pose{{s.at("x").get<float>(), s.at("y").get<float>()}, s.value("theta", 0.0f)};
    }
}

void parseTasks(const Json& j, TaskGenParams& t) {
    read(j, "count", t.count);
    read(j, "initial", t.initial);
    read(j, "arrivalInterval", t.arrivalInterval);
    if (j.contains("priorityWeights")) {
        const auto& w = j["priorityWeights"];
        if (w.is_array()) {
            for (size_t i = 0; i < 4 && i < w.size(); ++i) t.priorityWeights[i] = w[i].get<float>();
        } else {
            t.priorityWeights = {w.value("low", 0.0f), w.value("medium", 0.0f), w.value("high", 0.0f),
                                 w.value("critical", 0.0f)};
        }
    }
    read(j, "deadlines", t.deadlines);
    read(j, "deadlineBase", t.deadlineBase);
    read(j, "slackMin", t.slackMin);
    read(j, "slackMax", t.slackMax);
    read(j, "loadTypes", t.loadTypes);
}

ObstacleSpec parseObstacle(const Json& j) {
    ObstacleSpec o;
    o.kind = j.value("kind", std::string("worker")) == "forklift" ? ObstacleSpec::Kind::Forklift
                                                                    : ObstacleSpec::Kind::Worker;
    if (j.contains("route")) o.route = toVecList(j["route"]);
    read(j, "speed", o.speed);
    read(j, "pause", o.pause);
    read(j, "stochastic", o.stochastic);
    return o;
}

}  // namespace

Json loadJsonWithBase(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("no se pudo abrir " + path.string());
    Json j;
    try {
        j = Json::parse(in, nullptr, true, /*ignore_comments=*/true);
    } catch (const std::exception& e) {
        throw std::runtime_error("JSON inválido en " + path.string() + ": " + e.what());
    }
    if (auto b = j.find("base"); b != j.end()) {
        Json base = loadJsonWithBase(path.parent_path() / b->get<std::string>());
        j.erase("base");
        base.merge_patch(j);
        return base;
    }
    return j;
}

void parseShield(const Json& j, ShieldParams& s) {
    read(j, "enabled", s.enabled);
    read(j, "decel", s.decel);
    read(j, "marginWorker", s.marginWorker);
    read(j, "marginForklift", s.marginForklift);
    read(j, "marginStatic", s.marginStatic);
    read(j, "lookahead", s.lookahead);
    read(j, "range", s.range);
}

ScenarioConfig parseScenario(const Json& j) {
    ScenarioConfig c;
    read(j, "name", c.name);
    read(j, "description", c.description);
    if (j.contains("robot")) parseRobot(j["robot"], c.robot);
    if (j.contains("energy")) parseEnergy(j["energy"], c.energy);
    if (j.contains("sensors")) parseSensors(j["sensors"], c.sensors);
    if (j.contains("sim")) parseSim(j["sim"], c.sim);
    if (j.contains("reward")) parseReward(j["reward"], c.reward);
    if (j.contains("shield")) parseShield(j["shield"], c.shield);
    if (j.contains("map")) parseMap(j["map"], c.map);
    if (j.contains("tasks")) parseTasks(j["tasks"], c.tasks);
    if (j.contains("loadTypes")) {
        for (const auto& l : j["loadTypes"]) {
            LoadType t;
            t.name = l.at("name").get<std::string>();
            read(l, "mass", t.mass);
            read(l, "lateralLimit", t.lateralLimit);
            read(l, "longitudinalLimit", t.longitudinalLimit);
            read(l, "fragile", t.fragile);
            c.loadTypes.push_back(t);
        }
    }
    if (j.contains("obstacles")) {
        const auto& o = j["obstacles"];
        read(o, "minCount", c.obstacles.minCount);
        read(o, "maxCount", c.obstacles.maxCount);
        read(o, "forkliftFraction", c.obstacles.forkliftFraction);
        read(o, "speedMin", c.obstacles.speedMin);
        read(o, "speedMax", c.obstacles.speedMax);
        read(o, "stochasticFraction", c.obstacles.stochasticFraction);
        if (o.contains("fixed")) for (const auto& f : o["fixed"]) c.fixedObstacles.push_back(parseObstacle(f));
    }
    if (j.contains("randomization")) {
        const auto& r = j["randomization"];
        auto& d = c.randomization;
        read(r, "enabled", d.enabled);
        read(r, "friction", d.friction);
        read(r, "mass", d.mass);
        read(r, "motorForce", d.motorForce);
        read(r, "lidarNoiseMax", d.lidarNoiseMax);
        read(r, "obstacleSpeed", d.obstacleSpeed);
    }
    if (j.contains("acceptance")) c.acceptance = j["acceptance"];
    if (c.loadTypes.empty()) c.loadTypes.push_back({"estandar", 10.0f, 3.0f, 3.0f, false});
    c.raw = j;
    validate(c);
    return c;
}

ScenarioConfig loadScenario(const std::filesystem::path& path) {
    try {
        return parseScenario(loadJsonWithBase(path));
    } catch (const std::exception& e) {
        throw std::runtime_error(path.string() + ": " + e.what());
    }
}

void validate(const ScenarioConfig& c) {
    auto fail = [](const std::string& m) { throw std::runtime_error("configuración inválida: " + m); };
    if (c.map.width < 10 || c.map.height < 10 || c.map.width > 60 || c.map.height > 60)
        fail("el mapa debe medir entre 10 y 60 m por lado");
    if (c.robot.mass <= 0 || c.robot.maxWheelForce <= 0) fail("masa y fuerza del robot deben ser positivas");
    if (c.robot.maxForwardSpeed <= 0 || c.robot.maxAngularSpeed <= 0) fail("velocidades máximas deben ser positivas");
    if (c.sim.physicsDt <= 0 || c.sim.physicsDt > 0.05f) fail("physicsDt debe estar en (0, 0.05]");
    if (c.sim.physicsStepsPerAction < 1) fail("physicsStepsPerAction >= 1");
    if (c.sensors.lidarRays < 4 || c.sensors.lidarRays > 360) fail("lidarRays entre 4 y 360");
    if (c.tasks.count < 1 || c.tasks.count > 50) fail("tasks.count entre 1 y 50");
    if (c.tasks.initial < 1 || c.tasks.initial > c.tasks.count) fail("tasks.initial entre 1 y tasks.count");
    float wsum = 0;
    for (float w : c.tasks.priorityWeights) {
        if (w < 0) fail("pesos de prioridad no negativos");
        wsum += w;
    }
    if (wsum <= 0) fail("al menos un peso de prioridad positivo");
    if (c.energy.capacityWh <= 0) fail("capacidad de batería positiva");
    if (c.obstacles.minCount > c.obstacles.maxCount) fail("obstacles.minCount <= maxCount");
    for (const auto& name : c.tasks.loadTypes) {
        bool found = false;
        for (const auto& l : c.loadTypes) found |= l.name == name;
        if (!found) fail("tipo de carga desconocido: " + name);
    }
    for (const auto& l : c.loadTypes)
        if (l.mass < 0 || l.mass > c.robot.maxCarryMass) fail("masa de carga fuera de rango: " + l.name);
    if (!c.map.procedural) {
        if (c.map.pickups.empty() || c.map.dropoffs.empty()) fail("el mapa necesita puntos de recogida y destino");
        if (c.map.chargers.empty()) fail("el mapa necesita al menos una estación de recarga");
    }
}

}  // namespace wrl
