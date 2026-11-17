#include "warehouse/env/warehouse_env.hpp"

#include <stdexcept>

namespace wrl {

const char* toString(EndReason r) {
    switch (r) {
        case EndReason::None: return "en_curso";
        case EndReason::AllDelivered: return "cola_completada";
        case EndReason::BatteryDepleted: return "bateria_agotada";
        case EndReason::TimeLimit: return "tiempo_maximo";
        case EndReason::WorkerCollision: return "colision_operario";
        case EndReason::CollisionLimit: return "limite_colisiones";
        case EndReason::Stalled: return "sin_progreso";
    }
    return "?";
}

WarehouseEnv::WarehouseEnv(ScenarioConfig cfg) : base_(std::move(cfg)), cfg_(base_) {
    obsSize_ = observationSize(base_.sensors);
    obs_.assign(static_cast<size_t>(obsSize_), 0.0f);
    contactCooldown_.reserve(16);
    reset(0);
}

WarehouseEnv::~WarehouseEnv() = default;

float WarehouseEnv::frictionUnderRobot() const {
    return map_.frictionAt(robot_.position(), cfg_.robot.floorFriction) * frictionScale_;
}

void WarehouseEnv::applyRandomization() {
    cfg_ = base_;
    frictionScale_ = 1.0f;
    obstacleSpeedScale_ = 1.0f;
    const auto& d = base_.randomization;
    if (!d.enabled) return;
    auto factor = [&](float spread) { return rng_.uniform(1.0f - spread, 1.0f + spread); };
    cfg_.robot.mass *= factor(d.mass);
    cfg_.robot.maxWheelForce *= factor(d.motorForce);
    frictionScale_ = factor(d.friction);
    cfg_.sensors.lidarNoise = rng_.uniform(0.0f, d.lidarNoiseMax);
    obstacleSpeedScale_ = factor(d.obstacleSpeed);
}

void WarehouseEnv::buildEpisode() {
    bool ok = false;
    for (int attempt = 0; attempt < 20 && !ok; ++attempt) {
        map_ = buildMap(cfg_.map, rng_);
        grid_ = OccupancyGrid(map_, cfg_.sim.gridResolution, cfg_.sim.inflation);
        planner_ = AStarPlanner(&grid_, cfg_.sim.clearanceCost);

        // Se descartan los puntos inalcanzables desde el arranque del robot.
        const auto start = grid_.nearestFree(grid_.toCell(map_.robotStart.pos));
        if (!start) continue;
        const auto seen = grid_.reachable(*start);
        auto reachable = [&](Vec2 p) {
            const auto c = grid_.nearestFree(grid_.toCell(p), 3);
            return c && seen[static_cast<size_t>(grid_.index(*c))];
        };
        auto filter = [&](std::vector<Vec2>& pts) { std::erase_if(pts, [&](Vec2 p) { return !reachable(p); }); };
        filter(map_.pickups);
        filter(map_.dropoffs);
        filter(map_.chargers);
        ok = !map_.pickups.empty() && !map_.dropoffs.empty() && !map_.chargers.empty();
    }
    if (!ok) throw std::runtime_error("no se pudo generar un mapa válido para " + cfg_.name);

    world_ = std::make_unique<PhysicsWorld>();
    world_->buildStatic(map_);
    robot_.create(world_->id(), cfg_.robot, map_.robotStart);
    obstacles_.create(world_->id(), map_, planner_, cfg_.fixedObstacles, cfg_.obstacles, obstacleSpeedScale_, rng_);
    tasks_.generate(cfg_, map_, planner_, rng_);
}

void WarehouseEnv::reset(uint64_t seed, ScenarioConfig cfg) {
    if (observationSize(cfg.sensors) != obsSize_)
        throw std::runtime_error("el escenario " + cfg.name + " cambia el tamaño de la observación");
    base_ = std::move(cfg);
    reset(seed);
}

void WarehouseEnv::reset(uint64_t seed) {
    seed_ = seed;
    rng_.seed(seed);
    applyRandomization();
    sensors_.configure(cfg_.sensors);
    selector_.configure(cfg_);
    buildEpisode();

    time_ = 0.0;
    done_ = false;
    carrying_ = -1;
    batteryWh_ = cfg_.energy.capacityWh * cfg_.energy.initialFraction;
    dockTimer_ = 0.0f;
    charging_ = false;
    touching_ = false;
    inRestricted_ = false;
    accelFiltered_ = {};
    contactCooldown_.clear();
    metrics_ = {};
    metrics_.tasksTotal = static_cast<int>(tasks_.tasks().size());
    lastAction_ = {};
    rampV_ = rampW_ = 0.0f;
    actions_.clear();
    legActive_ = false;
    decision_ = {};
    pathVersion_ = 0;
    stallTimer_ = 0.0f;
    bestRemaining_ = 1e9f;

    tasks_.release(0.0);
    decide(true);
    updateCarrot();
    sensors_.scanLidar(*world_, robot_.pose(), rng_, lidar_);
    sensors_.perceiveObstacles(*world_, robot_.pose(), robot_.velocity(), obstacles_.obstacles(), perceived_);
    measuredV_ = measuredW_ = 0.0f;
    buildObservation();
}

void WarehouseEnv::physicsStep(float dt, float vCmd, float wCmd, StepEvents& ev, double& energyJ) {
    const Vec2 startPos = robot_.position();
    obstacles_.update(dt, startPos, planner_, map_, rng_);

    const Vec2 vBefore = robot_.velocity();
    // Rampa del controlador de motores (como en un AGV real): la consigna cambia
    // a ritmo acotado; la aceleración real sigue dependiendo de masa y adherencia.
    auto ramp = [dt](float current, float target, float rate) {
        return rate > 0.0f ? current + std::clamp(target - current, -rate * dt, rate * dt) : target;
    };
    // Conectado al cargador el robot queda acoplado: los motores no responden
    // hasta terminar la carga (si no, cualquier temblor de la política lo desconecta).
    if (charging_) vCmd = wCmd = 0.0f;
    rampV_ = ramp(rampV_, vCmd, cfg_.robot.commandAccel);
    rampW_ = ramp(rampW_, wCmd, cfg_.robot.commandAngularAccel);
    const auto power = robot_.applyControl(rampV_, rampW_, frictionUnderRobot(), dt, cfg_.energy);
    world_->step(dt, cfg_.sim.subSteps);
    time_ += dt;

    const double joules = static_cast<double>(power.electricalW) * dt;
    batteryWh_ = std::min(cfg_.energy.capacityWh, batteryWh_ - static_cast<float>(joules / 3600.0));
    if (joules > 0) {
        energyJ += joules;
        metrics_.energyWh += joules / 3600.0;
    }
    if (power.slipping) ++metrics_.slipSteps;

    const Pose pose = robot_.pose();
    metrics_.distance += distance(pose.pos, startPos);

    // Aceleración filtrada (lo que "siente" la carga).
    const Vec2 accel = (robot_.velocity() - vBefore) / dt;
    const float alpha = dt / (cfg_.sim.loadSmoothing + dt);
    accelFiltered_ += (accel - accelFiltered_) * alpha;
    metrics_.sumAbsAccel += accelFiltered_.length() * dt;
    if (carrying_ >= 0) {
        const LoadType& load = cfg_.loadTypes[static_cast<size_t>(tasks_.get(carrying_).loadType)];
        const Vec2 local = toLocal(accelFiltered_, pose.theta);
        if (std::abs(local.x) > load.longitudinalLimit || std::abs(local.y) > load.lateralLimit) {
            tasks_.markDropped(carrying_);
            robot_.setPayload(0.0f);
            carrying_ = -1;
            accelFiltered_ = {};
            ++ev.loadDrops;
            ++metrics_.loadIncidents;
            legActive_ = false;
            decide(true);
        }
    }

    handleContacts(ev, vBefore);

    const bool inR = map_.inRestricted(pose.pos);
    if (inR && !inRestricted_) ++ev.restrictedEntries;
    inRestricted_ = inR;

    if (batteryWh_ <= 0.0f) {
        batteryWh_ = 0.0f;
        if (!charging_) ev.batteryDepleted = true;
    }
}

void WarehouseEnv::handleContacts(StepEvents& ev, Vec2 vBefore) {
    for (const auto& c : world_->robotContacts()) {
        Vec2 vOther{};
        if ((c.other.tag == BodyTag::Forklift || c.other.tag == BodyTag::Worker) && c.other.index >= 0)
            vOther = obstacles_.obstacles()[static_cast<size_t>(c.other.index)].vel;
        const float approach = std::abs(dot(vBefore - vOther, c.normal));

        // Un mismo contacto que parpadea (rozando un muro) cuenta una sola vez.
        const uint32_t key = (static_cast<uint32_t>(c.other.tag) << 16) | static_cast<uint32_t>(c.other.index + 1);
        auto it = std::find_if(contactCooldown_.begin(), contactCooldown_.end(), [&](auto& e) { return e.first == key; });
        if (it != contactCooldown_.end()) {
            const bool recent = time_ - it->second < cfg_.sim.collisionCooldown;
            it->second = time_;
            if (recent) continue;
        } else {
            contactCooldown_.push_back({key, time_});
        }

        const auto& r = cfg_.reward;
        const Severity s = approach < r.grazeSpeed ? Severity::Graze : approach > r.severeSpeed ? Severity::Severe : Severity::Light;
        ++ev.collisions[static_cast<int>(s)];
        if (s == Severity::Severe && c.other.tag == BodyTag::Worker) ++ev.workerSevere;
    }
}

void WarehouseEnv::updateDocking(float dt, StepEvents& ev, float& reward) {
    const auto& sim = cfg_.sim;
    const Vec2 p = robot_.position();
    const bool inZone = distance(p, decision_.goal) < sim.dockRadius;
    const bool slow = robot_.velocity().length() < sim.dockSpeed && std::abs(robot_.angularSpeed()) < sim.dockAngularSpeed;

    if (decision_.phase == Phase::ToCharger || decision_.phase == Phase::Idle) {
        // Para empezar a cargar hay que acoplarse (zona + detenido); una vez
        // conectado, el contacto tolera pequeños movimientos hasta 2 radios.
        const bool keepsContact = charging_ && distance(p, decision_.goal) < 2.0f * sim.dockRadius;
        if (!(inZone && slow) && !keepsContact) {
            dockTimer_ = 0.0f;
            charging_ = false;
            return;
        }
        dockTimer_ += dt;
        if (dockTimer_ < sim.dockTime) return;
        if (!charging_) {
            charging_ = true;
            ++ev.rechargesStarted;
            if (decision_.phase == Phase::ToCharger && batteryFraction() > cfg_.energy.unnecessaryAbove)
                ++ev.unnecessaryRecharges;
        }
        batteryWh_ = std::min(cfg_.energy.capacityWh, batteryWh_ + cfg_.energy.chargePowerW * dt / 3600.0f);
        if (decision_.phase == Phase::ToCharger && batteryFraction() >= cfg_.energy.chargeTarget) {
            reward += cfg_.reward.chargeComplete;
            charging_ = false;
            dockTimer_ = 0.0f;
            decide(true);
        }
        return;
    }

    dockTimer_ = (inZone && slow) ? dockTimer_ + dt : 0.0f;
    if (dockTimer_ < sim.dockTime) return;
    dockTimer_ = 0.0f;
    const auto& rw = cfg_.reward;

    if (decision_.phase == Phase::ToPickup && decision_.taskId >= 0) {
        const int id = decision_.taskId;
        tasks_.markPicked(id, time_);
        carrying_ = id;
        robot_.setPayload(cfg_.loadTypes[static_cast<size_t>(tasks_.get(id).loadType)].mass);
        accelFiltered_ = {};
        ++ev.pickups;
        reward += rw.pickup;
        finishLeg();
        decide(true);
    } else if (decision_.phase == Phase::ToDropoff && carrying_ >= 0) {
        const int id = carrying_;
        const bool onTime = tasks_.markDelivered(id, time_);
        const Task& t = tasks_.get(id);
        robot_.setPayload(0.0f);
        carrying_ = -1;
        finishLeg();
        ++metrics_.delivered;
        metrics_.sumDeliveryTime += time_ - t.releaseTime;
        const bool critical = t.priority == Priority::Critical;
        const float base = critical ? rw.deliveryCriticalOnTime : rw.deliveryOnTime;
        if (onTime) {
            ++ev.deliveriesOnTime;
            ++metrics_.onTime;
            if (critical) ++ev.criticalOnTime;
            reward += base;
        } else {
            // Tarde vale menos, pero siempre más que no entregar: si una entrega
            // tardía restara, abandonar una tarea vencida sería lo "óptimo".
            const double delay = time_ - t.deadline;
            ++ev.deliveriesLate;
            ++metrics_.late;
            metrics_.sumDelay += delay;
            reward += base + std::max(rw.lateCap, rw.lateDelivery + rw.latePerSecond * static_cast<float>(delay));
        }
        decide(true);
    }
}

void WarehouseEnv::decide(bool force) {
    TaskSelector::Context ctx;
    ctx.robotPos = robot_.position();
    ctx.now = time_;
    ctx.batteryWh = batteryWh_;
    ctx.totalMass = robot_.totalMass();
    ctx.carryingTask = carrying_;
    const Decision d = selector_.decide(ctx, tasks_, map_);
    const bool same = d.phase == decision_.phase && d.taskId == decision_.taskId;
    if (same && !force) return;

    if (decision_.phase == Phase::ToPickup && decision_.taskId >= 0 && d.taskId != decision_.taskId)
        tasks_.unassign(decision_.taskId);
    if (d.phase == Phase::ToPickup) tasks_.assign(d.taskId);

    const bool goalChanged = !same || !(d.goal == decision_.goal);
    decision_ = d;
    if (goalChanged) {
        charging_ = false;
        stallTimer_ = 0.0f;
        bestRemaining_ = 1e9f;
        dockTimer_ = 0.0f;
        legActive_ = false;
        replan();
        startLeg();
    }
}

bool WarehouseEnv::stalled() {
    // Sin acercarse al objetivo (0.5 m) durante stallTimeout segundos. No cuenta
    // mientras recarga, espera tareas o está acoplando.
    if (cfg_.sim.stallTimeout <= 0.0f) return false;
    const float remaining = path_.remaining(robot_.position());
    const bool exempt = charging_ || decision_.phase == Phase::Idle || remaining < cfg_.sim.dockRadius;
    if (exempt || remaining < bestRemaining_ - 0.5f) {
        bestRemaining_ = remaining;
        stallTimer_ = 0.0f;
        return false;
    }
    stallTimer_ += controlDt();
    return stallTimer_ > cfg_.sim.stallTimeout;
}

void WarehouseEnv::replan() {
    const Vec2 pos = robot_.position();
    auto p = planner_.plan(pos, decision_.goal);
    path_ = p ? std::move(*p) : Path({pos, decision_.goal});
    replanTimer_ = 0.0f;
    ++pathVersion_;
    updateCarrot();
}

void WarehouseEnv::updateCarrot() {
    const auto proj = path_.project(robot_.position());
    carrot_ = path_.pointAt(proj.s + cfg_.sim.lookahead);
}

void WarehouseEnv::startLeg() {
    if (decision_.phase != Phase::ToPickup && decision_.phase != Phase::ToDropoff) return;
    legShortest_ = planner_.shortestLength(robot_.position(), decision_.goal).value_or(0.0f);
    legStartOdometer_ = metrics_.distance;
    legActive_ = legShortest_ > 0.5f;
}

void WarehouseEnv::finishLeg() {
    if (!legActive_) return;
    metrics_.legActual += metrics_.distance - legStartOdometer_;
    metrics_.legShortest += legShortest_;
    legActive_ = false;
}

float WarehouseEnv::potential() const {
    return -cfg_.reward.progressPerMeter * path_.remaining(robot_.position());
}

StepResult WarehouseEnv::step(const Action& action) {
    StepResult res;
    if (done_) {
        res.terminated = true;
        res.reason = metrics_.reason;
        return res;
    }
    Action a{std::isfinite(action.linear) ? std::clamp(action.linear, -1.0f, 1.0f) : 0.0f,
             std::isfinite(action.angular) ? std::clamp(action.angular, -1.0f, 1.0f) : 0.0f};
    if (recordActions_) actions_.push_back(a);

    const auto& rp = cfg_.robot;
    const float vCmd = a.linear >= 0 ? a.linear * rp.maxForwardSpeed : a.linear * rp.maxReverseSpeed;
    const float wCmd = a.angular * rp.maxAngularSpeed;
    const float dt = cfg_.sim.physicsDt;
    const auto& rw = cfg_.reward;
    StepEvents& ev = res.events;
    float reward = 0.0f;

    const float phi0 = potential();
    const int version0 = pathVersion_;
    double energyJ = 0.0;
    int touchingSteps = 0;
    for (int i = 0; i < cfg_.sim.physicsStepsPerAction; ++i) {
        physicsStep(dt, vCmd, wCmd, ev, energyJ);
        updateDocking(dt, ev, reward);
        touchingSteps += touching() ? 1 : 0;
        if (ev.batteryDepleted) break;
    }
    touching_ = touchingSteps > 0;

    // Nivel alto: llegadas, críticas que desplazan tareas aún no recogidas, batería baja.
    const int newCritical = tasks_.release(time_);
    ev.criticalOverdue = tasks_.collectNewCriticalOverdue(time_);
    if (carrying_ < 0) {
        const bool idleWithWork = decision_.phase == Phase::Idle && tasks_.openCount() > 0;
        // Vigilancia continua: ¿alcanza la batería para llegar a una estación con margen?
        const Vec2 pos = robot_.position();
        const float toCharger = distance(pos, selector_.nearestCharger(pos, map_)) * 1.35f;
        const float needed = selector_.energyForDistance(toCharger, robot_.totalMass()) +
                             cfg_.energy.reserveFraction * cfg_.energy.capacityWh;
        const bool lowBattery = (decision_.phase == Phase::ToPickup || decision_.phase == Phase::Idle) &&
                                !charging_ && batteryWh_ < needed;
        if (newCritical > 0 || idleWithWork || lowBattery) decide(false);
    }

    // Progreso por diferencia de potencial sobre la misma ruta (si cambió, no se mide).
    if (pathVersion_ == version0) reward += potential() - phi0;

    const Pose pose = robot_.pose();
    replanTimer_ += controlDt();
    if (replanTimer_ >= cfg_.sim.replanInterval || path_.project(pose.pos).lateral > cfg_.sim.offPathReplan) {
        replan();
        ev.replanned = true;
    }
    updateCarrot();

    sensors_.scanLidar(*world_, pose, rng_, lidar_);
    sensors_.perceiveObstacles(*world_, pose, robot_.velocity(), obstacles_.obstacles(), perceived_);
    measuredV_ = sensors_.noisySpeed(robot_.forwardSpeed(), rng_);
    measuredW_ = sensors_.noisySpeed(robot_.angularSpeed(), rng_);
    const float minLidar = *std::min_element(lidar_.begin(), lidar_.end());
    metrics_.minClearance = std::min(metrics_.minClearance, static_cast<double>(minLidar - rp.halfWidth));

    // Recompensa (sección 8.4).
    const float jerk = std::abs(a.linear - lastAction_.linear) + std::abs(a.angular - lastAction_.angular);
    reward += rw.timePerSecond * controlDt();
    reward += rw.energyPerKJ * static_cast<float>(energyJ / 1000.0);
    reward += rw.jerk * jerk;
    reward += rw.collisionGraze * static_cast<float>(ev.collisions[0]);
    reward += rw.collisionLight * static_cast<float>(ev.collisions[1]);
    reward += rw.collisionSevere * static_cast<float>(ev.collisions[2]);
    reward += rw.loadDestabilized * static_cast<float>(ev.loadDrops);
    reward += rw.criticalOverdue * static_cast<float>(ev.criticalOverdue);
    reward += rw.unnecessaryRecharge * static_cast<float>(ev.unnecessaryRecharges);
    reward += rw.restrictedEntry * static_cast<float>(ev.restrictedEntries);
    // Empujar contra algo y quedarse sin avanzar no salen gratis: sin esto, un
    // robot inmovilizado contra un muro es un estado "seguro" hasta el corte.
    reward += rw.contactPerSecond * dt * static_cast<float>(touchingSteps);
    if (stallTimer_ > rw.stuckGrace) reward += rw.stuckPerSecond * controlDt();
    lastAction_ = a;

    // Métricas.
    for (int s = 0; s < 3; ++s) metrics_.collisions[s] += ev.collisions[s];
    metrics_.workerSevere += ev.workerSevere;
    metrics_.criticalOverdue += ev.criticalOverdue;
    metrics_.recharges += ev.rechargesStarted;
    metrics_.unnecessaryRecharges += ev.unnecessaryRecharges;
    metrics_.restrictedEntries += ev.restrictedEntries;
    metrics_.sumJerk += jerk;
    metrics_.steps += 1;
    metrics_.simTime = time_;

    // Terminación.
    if (ev.batteryDepleted) {
        reward += rw.batteryDepleted;
        metrics_.batteryDepleted = true;
        res.terminated = true;
        res.reason = EndReason::BatteryDepleted;
    } else if (ev.workerSevere > 0) {
        res.terminated = true;
        res.reason = EndReason::WorkerCollision;
    } else if (metrics_.harmfulCollisions() >= cfg_.sim.maxCollisions) {
        res.terminated = true;
        res.reason = EndReason::CollisionLimit;
    } else if (tasks_.allDelivered()) {
        res.terminated = true;
        res.reason = EndReason::AllDelivered;
    } else if (stalled()) {
        // Es un fracaso, no un corte por tiempo: termina sin bootstrap y penaliza.
        // Si no, quedarse quieto (p. ej. con una carga frágil) sale "gratis".
        reward += rw.stalled;
        res.terminated = true;
        res.reason = EndReason::Stalled;
    } else if (time_ >= cfg_.sim.maxTime) {
        res.truncated = true;
        res.reason = EndReason::TimeLimit;
    }

    // Un fracaso deja tareas sin hacer: si no costaran nada, terminar el episodio
    // (por ejemplo, quedándose quieto) ahorraría los retrasos que aún faltan.
    if (res.terminated && res.reason != EndReason::AllDelivered)
        reward += rw.abandonedTask * static_cast<float>(metrics_.tasksTotal - metrics_.delivered);

    res.reward = reward;
    metrics_.totalReturn += reward;
    if (res.done()) {
        done_ = true;
        metrics_.reason = res.reason;
    }
    buildObservation();
    return res;
}

void WarehouseEnv::buildObservation() {
    float* o = obs_.data();
    const auto& rp = cfg_.robot;
    const auto& sp = cfg_.sensors;
    const Pose pose = robot_.pose();

    for (float r : lidar_) *o++ = r / sp.lidarRange;

    *o++ = std::clamp(measuredV_ / rp.maxForwardSpeed, -1.0f, 1.0f);
    *o++ = std::clamp(measuredW_ / rp.maxAngularSpeed, -1.0f, 1.0f);
    *o++ = lastAction_.linear;
    *o++ = lastAction_.angular;

    auto polar = [&](Vec2 target, float scale, bool squash) {
        const Vec2 rel = toLocal(target - pose.pos, pose.theta);
        const float d = rel.length();
        const float b = std::atan2(rel.y, rel.x);
        *o++ = squash ? std::tanh(d / scale) : std::min(d / scale, 1.0f);
        *o++ = std::sin(b);
        *o++ = std::cos(b);
        return d;
    };
    polar(carrot_, cfg_.sim.lookahead, false);
    const float goalDist = polar(decision_.goal, 5.0f, true);

    *o++ = goalDist < cfg_.sim.dockRadius ? 1.0f : 0.0f;
    *o++ = std::clamp(dockProgress(), 0.0f, 1.0f);

    *o++ = batteryFraction();
    *o++ = robot_.payload() / rp.maxCarryMass;
    for (int ph = 0; ph < 4; ++ph) *o++ = static_cast<int>(decision_.phase) == ph ? 1.0f : 0.0f;

    float slack = 1.0f, prio = 0.0f;
    if (decision_.taskId >= 0) {
        const Task& t = tasks_.get(decision_.taskId);
        if (t.hasDeadline()) slack = std::tanh(static_cast<float>(t.deadline - time_) / 60.0f);
        prio = static_cast<float>(t.priority) / 3.0f;
    }
    *o++ = slack;
    *o++ = prio;

    for (int i = 0; i < sp.nearestObstacles; ++i) {
        if (i < static_cast<int>(perceived_.size())) {
            const auto& p = perceived_[static_cast<size_t>(i)];
            *o++ = p.relPos.x / sp.lidarRange;
            *o++ = p.relPos.y / sp.lidarRange;
            *o++ = std::clamp(p.relVel.x / 2.0f, -1.5f, 1.5f);
            *o++ = std::clamp(p.relVel.y / 2.0f, -1.5f, 1.5f);
            *o++ = 1.0f;
        } else {
            for (int k = 0; k < 5; ++k) *o++ = 0.0f;
        }
    }
    *o++ = std::clamp(robot_.lateralSpeed() / 0.5f, -1.0f, 1.0f);

    // Lo que "siente" la carga (IMU): aceleración actual relativa a su límite.
    float latStress = 0.0f, longStress = 0.0f;
    if (carrying_ >= 0) {
        const LoadType& load = cfg_.loadTypes[static_cast<size_t>(tasks_.get(carrying_).loadType)];
        const Vec2 local = toLocal(accelFiltered_, pose.theta);
        latStress = std::min(std::abs(local.y) / load.lateralLimit, 1.5f);
        longStress = std::min(std::abs(local.x) / load.longitudinalLimit, 1.5f);
    }
    *o++ = latStress;
    *o++ = longStress;

    // Estado que antes era invisible para la política (el corte por falta de
    // progreso dependía de un temporizador oculto).
    *o++ = cfg_.sim.stallTimeout > 0.0f ? std::min(stallTimer_ / cfg_.sim.stallTimeout, 1.0f) : 0.0f;
    *o++ = charging_ ? 1.0f : 0.0f;
    *o++ = touching_ ? 1.0f : 0.0f;
}

bool WarehouseEnv::touching() const {
    b2ContactData buf[8];
    const int n = b2Body_GetContactData(robot_.body(), buf, 8);
    for (int i = 0; i < n; ++i)
        if (buf[i].manifold.pointCount > 0) return true;
    return false;
}

float WarehouseEnv::loadStress() const {
    if (carrying_ < 0) return 0.0f;
    const LoadType& load = cfg_.loadTypes[static_cast<size_t>(tasks_.get(carrying_).loadType)];
    const Vec2 local = toLocal(accelFiltered_, robot_.pose().theta);
    return std::max(std::abs(local.y) / load.lateralLimit, std::abs(local.x) / load.longitudinalLimit);
}

}  // namespace wrl
