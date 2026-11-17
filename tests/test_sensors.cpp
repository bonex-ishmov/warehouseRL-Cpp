#include <gtest/gtest.h>

#include "warehouse/physics/robot_model.hpp"
#include "warehouse/sensors/sensor_suite.hpp"

using namespace wrl;

TEST(Sensores, LidarMideLaDistanciaAlMuro) {
    PhysicsWorld world;
    WarehouseMap m;
    m.width = 20;
    m.height = 20;
    m.shelves.push_back({13, 5, 1, 10});
    world.buildStatic(m);
    RobotModel robot;
    robot.create(world.id(), RobotParams{}, {{10, 10}, 0});

    SensorParams sp;
    sp.lidarRays = 36;
    sp.lidarNoise = 0.0f;
    SensorSuite s;
    s.configure(sp);
    Rng rng(1);
    std::vector<float> scan;
    s.scanLidar(world, robot.pose(), rng, scan);
    ASSERT_EQ(scan.size(), 36u);
    // El rayo 18 apunta hacia delante (ángulo 0): el muro está a 3 m del centro.
    EXPECT_NEAR(s.rayAngle(18), 0.0f, 1e-5f);
    EXPECT_NEAR(scan[18], 3.0f, 0.02f);
    // Hacia atrás no hay nada dentro del alcance (el propio robot no se detecta).
    EXPECT_FLOAT_EQ(scan[0], sp.lidarRange);
}

TEST(Sensores, ElRuidoEsReproducibleConLaMismaSemilla) {
    PhysicsWorld world;
    WarehouseMap m;
    m.width = 20;
    m.height = 20;
    m.shelves.push_back({13, 5, 1, 10});
    world.buildStatic(m);
    SensorParams sp;
    sp.lidarNoise = 0.05f;
    SensorSuite s;
    s.configure(sp);
    Rng a(42), b(42);
    std::vector<float> s1, s2;
    s.scanLidar(world, {{10, 10}, 0}, a, s1);
    s.scanLidar(world, {{10, 10}, 0}, b, s2);
    EXPECT_EQ(s1, s2);
}
