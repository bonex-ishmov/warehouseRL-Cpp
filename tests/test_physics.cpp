#include <gtest/gtest.h>

#include "warehouse/physics/physics_world.hpp"
#include "warehouse/physics/robot_model.hpp"

using namespace wrl;

namespace {

struct Rig {
    PhysicsWorld world;
    RobotModel robot;
    RobotParams params;
    EnergyParams energy;
    float dt = 1.0f / 60.0f;

    explicit Rig(float payload = 0.0f) {
        robot.create(world.id(), params, {{10, 10}, 0});
        robot.setPayload(payload);
    }
    void run(float seconds, float v, float w, float mu = 0.8f) {
        const int n = static_cast<int>(seconds / dt + 0.5f);
        for (int i = 0; i < n; ++i) {
            robot.applyControl(v, w, mu, dt, energy);
            world.step(dt, 4);
        }
    }
};

}  // namespace

TEST(Fisica, AceleracionCoincideConElModeloAnalitico) {
    Rig rig;
    rig.run(0.5f, 1.5f, 0.0f);
    // Fuerza neta = 2 (Fmax - rodadura) ; a = F / m.
    const auto& p = rig.params;
    const float rolling = p.rollingResistance * 0.25f * p.mass * kGravity * 2.0f;
    const float a = 2.0f * (p.maxWheelForce - rolling) / p.mass;
    EXPECT_NEAR(rig.robot.forwardSpeed(), a * 0.5f, 0.05f * a * 0.5f);
}

TEST(Fisica, LaVelocidadSeSaturaEnElMaximo) {
    Rig rig;
    rig.run(4.0f, 5.0f, 0.0f);
    EXPECT_NEAR(rig.robot.forwardSpeed(), rig.params.maxForwardSpeed, 0.05f);
}

TEST(Fisica, LaCargaReduceLaAceleracion) {
    Rig empty(0.0f), loaded(30.0f);
    empty.run(0.5f, 1.5f, 0.0f);
    loaded.run(0.5f, 1.5f, 0.0f);
    EXPECT_GT(empty.robot.forwardSpeed(), loaded.robot.forwardSpeed() * 1.3f);
    EXPECT_FLOAT_EQ(loaded.robot.totalMass(), 80.0f);
}

TEST(Fisica, LaCargaAlargaLaDistanciaDeFrenado) {
    auto brake = [](float payload) {
        Rig rig(payload);
        rig.run(3.0f, 1.0f, 0.0f);
        const float x0 = rig.robot.position().x;
        rig.run(3.0f, 0.0f, 0.0f);
        EXPECT_LT(std::abs(rig.robot.forwardSpeed()), 0.02f);
        return rig.robot.position().x - x0;
    };
    const float light = brake(0.0f), heavy = brake(30.0f);
    EXPECT_GT(heavy, light * 1.3f);
}

TEST(Fisica, PocaAdherenciaProduceDerrape) {
    // Llega a 1.5 m/s con buen agarre y gira fuerte al entrar en una zona con adherencia mu.
    auto slip = [](float mu) {
        Rig rig;
        rig.run(2.0f, 1.5f, 0.0f, 0.8f);
        float maxLat = 0.0f;
        for (int i = 0; i < 60; ++i) {
            rig.run(rig.dt, 1.5f, 2.0f, mu);
            maxLat = std::max(maxLat, std::abs(rig.robot.lateralSpeed()));
        }
        return maxLat;
    };
    EXPECT_GT(slip(0.15f), 3.0f * slip(0.8f));
}

TEST(Fisica, ConsumoEnReposoEsLaPotenciaBase) {
    Rig rig;
    const auto p = rig.robot.applyControl(0.0f, 0.0f, 0.8f, rig.dt, rig.energy);
    EXPECT_NEAR(p.electricalW, rig.energy.basePowerW, 1e-3f);
}

TEST(Fisica, AcelerarConsumeMasQueRodarAVelocidadConstante) {
    Rig rig;
    float accel = 0.0f, cruise = 0.0f;
    for (int i = 0; i < 20; ++i) {
        accel += rig.robot.applyControl(1.5f, 0.0f, 0.8f, rig.dt, rig.energy).electricalW;
        rig.world.step(rig.dt, 4);
    }
    rig.run(4.0f, 1.0f, 0.0f);
    for (int i = 0; i < 20; ++i) {
        cruise += rig.robot.applyControl(1.0f, 0.0f, 0.8f, rig.dt, rig.energy).electricalW;
        rig.world.step(rig.dt, 4);
    }
    EXPECT_GT(accel, cruise);
}

TEST(Fisica, ElRobotNoAtraviesaUnMuro) {
    Rig rig;
    WarehouseMap m;
    m.width = 20;
    m.height = 20;
    m.shelves.push_back({12, 8, 0.2f, 4});  // muro delgado delante del robot
    rig.world.buildStatic(m);
    rig.run(5.0f, 1.5f, 0.0f);
    EXPECT_LT(rig.robot.position().x, 12.0f);
}
