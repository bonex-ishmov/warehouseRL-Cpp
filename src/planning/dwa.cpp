#include "warehouse/planning/dwa.hpp"

namespace wrl {

DWAController::DWAController(const RobotParams& robot, Params params) : robot_(robot), p_(params) {}

DWAController::Command DWAController::compute(const Input& in) {
    const float goalDist = distance(in.pose.pos, in.goal);
    // Dentro de la zona de acoplamiento: detenerse.
    if (goalDist < in.dockRadius * 0.6f) {
        stuck_ = recovery_ = 0.0f;
        turnDir_ = 0;
        anchor_ = in.pose.pos;
        return {0.0f, 0.0f};
    }

    const bool aimAtGoal = distance(in.carrot, in.goal) < 0.05f;
    const Vec2 target = aimAtGoal ? in.goal : in.carrot;
    const Vec2 rel = toLocal(target - in.pose.pos, in.pose.theta);
    const float bearing = std::atan2(rel.y, rel.x);

    // Nube de puntos del LiDAR (solo impactos reales).
    points_.clear();
    const auto& lidar = *in.lidar;
    const float range = in.sensors->params().lidarRange;
    float circleClear = 10.0f;
    for (size_t i = 0; i < lidar.size(); ++i) {
        circleClear = std::min(circleClear, lidar[i] - robot_.radius());
        if (lidar[i] >= range * 0.98f) continue;
        const float a = in.pose.theta + in.sensors->rayAngle(static_cast<int>(i));
        points_.push_back(in.pose.pos + fromAngle(a) * lidar[i]);
    }
    const bool canSpin = circleClear > 0.03f;

    // Objetivo a la espalda: girar en el sitio con sentido fijo (si no, el rumbo
    // salta entre +pi y -pi y el robot oscila sin decidirse).
    if (turnDir_ == 0 && std::abs(bearing) > 2.0f && std::abs(in.v) < 0.3f && canSpin) turnDir_ = bearing >= 0 ? 1 : -1;
    if (turnDir_ != 0) {
        if (std::abs(bearing) < 0.6f || !canSpin) turnDir_ = 0;
        else return {0.0f, static_cast<float>(turnDir_) * 1.2f};
    }

    // Recuperación tras un atasco: girar hacia el objetivo o, si no hay sitio, retroceder.
    if (recovery_ > 0.0f) {
        recovery_ -= in.dt;
        if (!canSpin) return {-0.2f, 0.0f};
        if (std::abs(bearing) > 0.25f) return {0.0f, std::copysign(std::min(1.2f, 2.0f * std::abs(bearing)), bearing)};
        recovery_ = 0.0f;
    }

    const Command cmd = search(in, target, aimAtGoal, goalDist);

    // Atasco: no avanzar 0.3 m en stuckTime segundos lejos del objetivo.
    if (distance(in.pose.pos, anchor_) > 0.3f) {
        anchor_ = in.pose.pos;
        stuck_ = 0.0f;
    } else {
        stuck_ += in.dt;
    }
    if (stuck_ > p_.stuckTime) {
        stuck_ = 0.0f;
        recovery_ = p_.recoveryTime;
    }
    return cmd;
}

DWAController::Command DWAController::search(const Input& in, Vec2 target, bool aimAtGoal, float goalDist) {
    // Ventana dinámica a partir de la aceleración que permiten motor, adherencia y carga.
    const float grip = std::min(robot_.maxWheelForce, in.friction * 0.25f * in.totalMass * kGravity);
    const float aMax = std::min(2.0f * grip / in.totalMass * 0.85f, in.maxAccel);
    const float inertia = in.totalMass * (4 * robot_.halfLength * robot_.halfLength + 4 * robot_.halfWidth * robot_.halfWidth) / 12.0f;
    const float alphaMax = 2.0f * grip * robot_.trackWidth * 0.5f / inertia * 0.7f;
    // Aceleración lateral admisible: la menor entre la adherencia y lo que tolera la carga.
    const float latMax = std::min(in.friction * kGravity * 0.6f, in.maxAccel);

    float vHi = std::min(robot_.maxForwardSpeed, in.v + aMax * p_.window);
    const float vLo = std::max(-robot_.maxReverseSpeed, in.v - aMax * p_.window);
    // Cerca del objetivo se limita la velocidad para poder acoplar.
    if (goalDist < p_.goalSlowdown) vHi = std::min(vHi, std::max(0.15f, 0.7f * goalDist));
    const float wLo = std::max(-robot_.maxAngularSpeed, in.w - alphaMax * p_.window);
    const float wHi = std::min(robot_.maxAngularSpeed, in.w + alphaMax * p_.window);

    // Distancia a la huella rectangular real (el círculo envolvente sobreestima
    // el ancho y bloquea al robot junto a las estanterías).
    auto footprint = [&](const Pose& s, float c, float sn, Vec2 q) {
        const Vec2 d = q - s.pos;
        const float lx = std::abs(c * d.x + sn * d.y) - robot_.halfLength;
        const float ly = std::abs(-sn * d.x + c * d.y) - robot_.halfWidth;
        const float ex = std::max(lx, 0.0f), ey = std::max(ly, 0.0f);
        return (ex > 0 || ey > 0) ? std::sqrt(ex * ex + ey * ey) : std::max(lx, ly);
    };
    float c0 = 10.0f;
    {
        const float c = std::cos(in.pose.theta), sn = std::sin(in.pose.theta);
        for (const auto& q : points_) c0 = std::min(c0, footprint(in.pose, c, sn, q));
        if (in.obstacles)
            for (const auto& o : *in.obstacles) c0 = std::min(c0, footprint(in.pose, c, sn, o.worldPos) - o.radius);
    }
    // Si ya está por debajo del margen (pegado a una estantería), se admiten las
    // trayectorias que no empeoran la holgura actual para poder salir.
    const float safety = std::min(p_.safety, c0 - 0.01f);

    Command best{};
    float bestScore = -1e9f;
    bool found = false;
    const int steps = static_cast<int>(p_.horizon / p_.simDt);
    const float d0 = distance(in.pose.pos, target);

    for (int iv = 0; iv < p_.linearSamples; ++iv) {
        const float v = vLo + (vHi - vLo) * static_cast<float>(iv) / static_cast<float>(std::max(1, p_.linearSamples - 1));
        for (int iw = 0; iw < p_.angularSamples; ++iw) {
            const float w = wLo + (wHi - wLo) * static_cast<float>(iw) / static_cast<float>(std::max(1, p_.angularSamples - 1));
            if (std::abs(v * w) > latMax) continue;

            Pose s = in.pose;
            Pose closest = in.pose;
            float closestDist = d0;
            float minClear = 10.0f;
            for (int k = 1; k <= steps; ++k) {
                s.theta += w * p_.simDt;
                s.pos += fromAngle(s.theta) * (v * p_.simDt);
                const float t = static_cast<float>(k) * p_.simDt;
                if (const float dt = distance(s.pos, target); dt < closestDist) {
                    closestDist = dt;
                    closest = s;
                }
                const float c = std::cos(s.theta), sn = std::sin(s.theta);
                for (const auto& q : points_) minClear = std::min(minClear, footprint(s, c, sn, q));
                if (in.obstacles)
                    for (const auto& o : *in.obstacles)
                        minClear = std::min(minClear, footprint(s, c, sn, o.worldPos + o.worldVel * t) - o.radius);
                if (minClear < safety) break;
            }
            if (minClear < safety) continue;
            // Debe poder frenar antes de chocar.
            if (v > 0 && v > std::sqrt(2.0f * aMax * std::max(0.0f, minClear - safety))) continue;

            // Hacia el objetivo final cuenta el punto de máxima aproximación (no
            // pasarse de largo); hacia el punto de seguimiento, el estado final.
            const Pose& ref = aimAtGoal ? closest : s;
            const float endDist = aimAtGoal ? closestDist : distance(s.pos, target);
            float heading = 1.0f;
            if (!(aimAtGoal && closestDist < in.dockRadius * 0.5f)) {
                const float bearing = std::abs(wrapAngle(std::atan2(target.y - ref.pos.y, target.x - ref.pos.x) - ref.theta));
                heading = 1.0f - bearing / kPi;
            }
            const float progress = (d0 - endDist) / (robot_.maxForwardSpeed * p_.horizon);
            const float clear = std::min(minClear, 1.5f) / 1.5f;
            const float speed = v / robot_.maxForwardSpeed;
            const float offPath = in.path && !in.path->empty() ? std::min(in.path->project(s.pos).lateral, 1.0f) : 0.0f;
            const float score = p_.headingWeight * heading + p_.clearanceWeight * clear + p_.speedWeight * speed +
                                p_.progressWeight * progress - p_.pathWeight * offPath;
            if (score > bestScore) {
                bestScore = score;
                best = {v, w};
                found = true;
            }
        }
    }
    if (!found) {
        // Sin trayectoria segura: frenar y girar hacia el objetivo.
        const Vec2 rel = toLocal(target - in.pose.pos, in.pose.theta);
        return {0.0f, std::clamp(std::atan2(rel.y, rel.x), -0.8f, 0.8f)};
    }
    return best;
}

}  // namespace wrl
