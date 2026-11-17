#include "warehouse/physics/robot_model.hpp"

#include "warehouse/physics/physics_world.hpp"

namespace wrl {

void RobotModel::create(b2WorldId world, const RobotParams& params, Pose start, int robotIndex) {
    params_ = params;
    payload_ = 0.0f;
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    bd.position = toB2(start.pos);
    bd.rotation = b2MakeRot(start.theta);
    bd.isBullet = true;  // detección continua frente a otros cuerpos móviles
    bd.angularDamping = params.angularDamping;
    body_ = b2CreateBody(world, &bd);

    b2ShapeDef sd = b2DefaultShapeDef();
    sd.density = params.mass / (4.0f * params.halfLength * params.halfWidth);
    sd.material.friction = params.bodyFriction;
    sd.material.restitution = params.restitution;
    sd.filter.categoryBits = kCatRobot;
    sd.filter.maskBits = kCatStatic | kCatObstacle;
    sd.enableContactEvents = true;
    sd.userData = ShapeTag::encode(BodyTag::Robot, robotIndex);
    const b2Polygon box = b2MakeRoundedBox(params.halfLength - 0.02f, params.halfWidth - 0.02f, 0.02f);
    b2CreatePolygonShape(body_, &sd, &box);
    updateMassData();
}

void RobotModel::setParams(const RobotParams& params) {
    params_ = params;
    b2Body_SetAngularDamping(body_, params.angularDamping);
    updateMassData();
}

void RobotModel::setPayload(float mass) {
    payload_ = std::max(0.0f, mass);
    updateMassData();
}

void RobotModel::updateMassData() {
    // La carga se modela como una caja con la misma huella, centrada.
    const float m = totalMass();
    const float a = 2.0f * params_.halfLength, b = 2.0f * params_.halfWidth;
    b2MassData md{};
    md.mass = m;
    md.center = {0.0f, 0.0f};
    md.rotationalInertia = m * (a * a + b * b) / 12.0f;
    b2Body_SetMassData(body_, md);
}

RobotModel::StepPower RobotModel::applyControl(float linearCmd, float angularCmd, float mu, float dt,
                                               const EnergyParams& energy) {
    const float v = std::clamp(linearCmd, -params_.maxReverseSpeed, params_.maxForwardSpeed);
    const float w = std::clamp(angularCmd, -params_.maxAngularSpeed, params_.maxAngularSpeed);
    const float half = params_.trackWidth * 0.5f;
    const float m = totalMass();
    const Pose p = pose();
    const Vec2 fwd = fromAngle(p.theta);
    const Vec2 left{-fwd.y, fwd.x};

    // Las ruedas motrices soportan la mitad del peso; las ruedas locas el resto.
    const float normalPerWheel = 0.25f * m * kGravity;
    const float grip = mu * normalPerWheel;
    StepPower out;

    for (int side = 0; side < 2; ++side) {
        const float sign = side == 0 ? 1.0f : -1.0f;  // 0 = izquierda (+y local)
        const Vec2 wheel = p.pos + left * (sign * half);
        const Vec2 vel = fromB2(b2Body_GetWorldPointVelocity(body_, toB2(wheel)));
        const float vF = dot(vel, fwd);
        const float vL = dot(vel, left);
        const float target = v - sign * w * half;

        // Fuerza longitudinal: lazo P saturado por el motor y por la adherencia.
        const float maxF = std::min(params_.maxWheelForce, grip);
        float F = std::clamp(params_.speedGain * (target - vF), -maxF, maxF);
        if (std::abs(vF) > 0.01f) F -= std::copysign(params_.rollingResistance * normalPerWheel * 2.0f, vF);
        b2Body_ApplyForce(body_, toB2(fwd * F), toB2(wheel), true);

        // Agarre lateral limitado por lo que queda del círculo de fricción.
        const float lateralGrip = 2.0f * std::sqrt(std::max(0.0f, grip * grip - F * F));
        const float needed = 0.5f * m * std::abs(vL);
        const float maxImpulse = lateralGrip * dt;
        const float J = std::min(needed, maxImpulse);
        if (needed > maxImpulse * 1.05f && std::abs(vL) > 0.05f) out.slipping = true;
        b2Body_ApplyLinearImpulse(body_, toB2(left * (-std::copysign(J, vL))), toB2(wheel), true);

        const float power = F * vF;
        out.mechanicalW += std::abs(power);
        if (power >= 0.0f || energy.regenEfficiency <= 0.0f)
            out.electricalW += std::abs(power) / energy.efficiency;
        else
            out.electricalW -= std::abs(power) * energy.regenEfficiency;
    }
    out.electricalW += energy.basePowerW;
    return out;
}

Pose RobotModel::pose() const {
    return {fromB2(b2Body_GetPosition(body_)), b2Rot_GetAngle(b2Body_GetRotation(body_))};
}
Vec2 RobotModel::position() const { return fromB2(b2Body_GetPosition(body_)); }
Vec2 RobotModel::velocity() const { return fromB2(b2Body_GetLinearVelocity(body_)); }
Vec2 RobotModel::forward() const { return fromAngle(pose().theta); }
float RobotModel::forwardSpeed() const { return dot(velocity(), forward()); }
float RobotModel::lateralSpeed() const {
    const Vec2 f = forward();
    return dot(velocity(), Vec2{-f.y, f.x});
}
float RobotModel::angularSpeed() const { return b2Body_GetAngularVelocity(body_); }

}  // namespace wrl
