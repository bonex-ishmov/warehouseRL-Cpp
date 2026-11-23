#include "warehouse/env/safety_shield.hpp"

#include <algorithm>
#include <cmath>

#include <box2d/collision.h>

#include "warehouse/physics/physics_world.hpp"

namespace wrl {

float SafetyShield::allowedClosing(float room, float decel, float controlDt) {
    // Recorre v·T mientras mantiene la consigna y luego frena a 'decel':
    // v·T + v²/(2a) <= room  ->  v = a·(sqrt(T² + 2·room/a) − T).
    if (room <= 0.0f) return 0.0f;
    return decel * (std::sqrt(controlDt * controlDt + 2.0f * room / decel) - controlDt);
}

SafetyShield::Result SafetyShield::filter(const ShieldParams& p, const RobotParams& rp, const WarehouseMap& map,
                                          const std::vector<DynamicObstacle>& obstacles, Pose pose, float v, float w,
                                          float controlDt, float vNow, float loadLateralLimit) const {
    if (!p.enabled) return {v, w, false, false};
    Result out = avoidObstacles(p, rp, map, obstacles, pose, v, w, controlDt);

    // Carga: en curva la aceleración lateral es v·giro. La mayoría de las caídas sin
    // choque eran por cerrar la curva o acelerar en ella. Se escalan avance y giro juntos
    // (se conserva la curva pedida y, como las restricciones de obstáculos son lineales
    // con cota >= 0, siguen cumpliéndose). Cuenta también la velocidad actual: la rampa
    // tarda en frenar, y mientras tanto el giro ya habría subido.
    const float limit = p.loadLateralFraction * loadLateralLimit;
    if (limit > 0.0f && std::abs(out.w) > 1e-3f) {
        auto lateral = [&](float t) { return t * std::abs(out.w) * std::max(std::abs(vNow), t * std::abs(out.v)); };
        if (lateral(1.0f) > limit) {
            float lo = 0.0f, hi = 1.0f;
            for (int i = 0; i < 20; ++i) {
                const float mid = 0.5f * (lo + hi);
                (lateral(mid) <= limit ? lo : hi) = mid;
            }
            out.v *= lo;
            out.w *= lo;
            out.limited = out.loadLimited = true;
        }
    }
    return out;
}

SafetyShield::Result SafetyShield::avoidObstacles(const ShieldParams& p, const RobotParams& rp, const WarehouseMap& map,
                                                  const std::vector<DynamicObstacle>& obstacles, Pose pose, float v,
                                                  float w, float controlDt) const {
    Result out{v, w, false, false};

    const Vec2 heading = fromAngle(pose.theta);
    const b2Vec2 robotPts[4] = {{-rp.halfLength, -rp.halfWidth}, {rp.halfLength, -rp.halfWidth},
                                {rp.halfLength, rp.halfWidth}, {-rp.halfLength, rp.halfWidth}};
    b2DistanceInput in{};
    in.proxyA = b2MakeProxy(robotPts, 4, 0.0f);
    in.transformA = {toB2(pose.pos), b2MakeRot(pose.theta)};
    in.useRadii = true;
    const float reach = rp.radius() + p.range;

    // Cada obstáculo cercano da una restricción sobre (v, w): la velocidad del punto del
    // robot más cercano (v·rumbo + w × brazo) proyectada hacia el obstáculo no puede
    // superar lo que permite frenar en el hueco que queda.
    cons_.clear();
    auto add = [&](const b2DistanceOutput& o, float margin, float obstacleIn) {
        if (o.distance < 1e-4f) return;  // ya en contacto: la normal no está definida
        const Vec2 n = normalized(fromB2(o.normal));
        const Vec2 arm = fromB2(o.pointA) - pose.pos;
        const float room = o.distance - margin - std::max(0.0f, obstacleIn) * p.lookahead;
        const float alpha = dot(heading, n);
        const float beta = dot(Vec2{-arm.y, arm.x}, n);
        cons_.push_back({alpha, beta, allowedClosing(room, p.decel, controlDt)});
    };

    in.transformB = b2Transform_identity;
    auto addRect = [&](const Rect& r) {
        if (r.distanceTo(pose.pos) > reach) return;
        const b2Vec2 pts[4] = {{r.x, r.y}, {r.x + r.w, r.y}, {r.x + r.w, r.y + r.h}, {r.x, r.y + r.h}};
        in.proxyB = b2MakeProxy(pts, 4, 0.0f);
        b2SimplexCache cache{};
        add(b2ShapeDistance(&in, &cache, nullptr, 0), p.marginStatic, 0.0f);
    };
    for (const auto& r : map.walls) addRect(r);
    for (const auto& r : map.shelves) addRect(r);

    for (const auto& ob : obstacles) {
        if ((ob.pos - pose.pos).length() > reach + ob.halfExtents.length()) continue;
        if (ob.isWorker()) {
            const b2Vec2 c{0.0f, 0.0f};
            in.proxyB = b2MakeProxy(&c, 1, ob.halfExtents.x);
        } else {
            const b2Vec2 pts[4] = {{-ob.halfExtents.x, -ob.halfExtents.y}, {ob.halfExtents.x, -ob.halfExtents.y},
                                   {ob.halfExtents.x, ob.halfExtents.y}, {-ob.halfExtents.x, ob.halfExtents.y}};
            in.proxyB = b2MakeProxy(pts, 4, 0.0f);
        }
        in.transformB = {toB2(ob.pos), b2MakeRot(ob.theta)};
        b2SimplexCache cache{};
        const b2DistanceOutput o = b2ShapeDistance(&in, &cache, nullptr, 0);
        const float obstacleIn = o.distance > 1e-4f ? -dot(ob.vel, normalized(fromB2(o.normal))) : 0.0f;
        add(o, ob.isWorker() ? p.marginWorker : p.marginForklift, obstacleIn);
    }

    auto feasible = [&](float vv, float ww) {
        for (const auto& c : cons_)
            if (c.alpha * vv + c.beta * ww > c.bound + 1e-4f) return false;
        return true;
    };
    if (feasible(v, w)) return out;
    out.limited = true;

    // 1) Recortar solo la velocidad de avance, conservando el giro pedido (la política
    //    puede estar girando justamente para esquivar).
    float lo = -rp.maxReverseSpeed, hi = rp.maxForwardSpeed;
    bool turnOk = true;
    for (const auto& c : cons_) {
        const float rhs = c.bound - c.beta * w;
        if (std::abs(c.alpha) < 1e-3f) {
            if (rhs < -1e-4f) turnOk = false;
        } else if (c.alpha > 0.0f) {
            hi = std::min(hi, rhs / c.alpha);
        } else {
            lo = std::max(lo, rhs / c.alpha);
        }
    }
    if (turnOk && lo <= hi) {
        out.v = std::clamp(v, lo, hi);
        if (feasible(out.v, w)) return out;
    }

    // 2) Si ni siquiera parado se puede girar así (una esquina barre la estantería), la
    //    consigna factible más parecida a la pedida: por ejemplo girar hacia el otro lado o
    //    retroceder. Escalar avance y giro juntos hasta (0, 0) dejaba al robot bloqueado
    //    contra la estantería mientras la política seguía pidiendo lo mismo.
    const float vScale = rp.maxForwardSpeed, wScale = rp.maxAngularSpeed;
    float best = 1e30f, bv = 0.0f, bw = 0.0f;
    constexpr int kV = 36, kW = 40;
    for (int i = 0; i <= kV; ++i) {
        const float vv = -rp.maxReverseSpeed + (rp.maxForwardSpeed + rp.maxReverseSpeed) * static_cast<float>(i) / kV;
        for (int j = 0; j <= kW; ++j) {
            const float ww = -rp.maxAngularSpeed + 2.0f * rp.maxAngularSpeed * static_cast<float>(j) / kW;
            const float dv = (vv - v) / vScale, dw = (ww - w) / wScale;
            const float cost = dv * dv + dw * dw;
            if (cost < best && feasible(vv, ww)) best = cost, bv = vv, bw = ww;
        }
    }
    out.v = bv;
    out.w = bw;
    return out;
}

}  // namespace wrl
