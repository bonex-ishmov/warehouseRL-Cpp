#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include "warehouse/sim/task_manager.hpp"
#include "warehouse/sim/task_selector.hpp"

using namespace wrl;

namespace {
Task makeTask(int id, Priority p, double release, double deadline, Vec2 pickup) {
    Task t;
    t.id = id;
    t.priority = p;
    t.releaseTime = release;
    t.deadline = deadline;
    t.pickup = pickup;
    t.destination = {8, 9};
    t.routeLength = 6.0f;
    return t;
}
}  // namespace

TEST(Tareas, SeLiberanCuandoLlegaSuInstante) {
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Low, 0, 1e9, {1, 1}), makeTask(1, Priority::Critical, 10, 1e9, {1, 1})});
    EXPECT_EQ(tm.release(0.0), 0);
    EXPECT_EQ(tm.get(1).status, TaskStatus::Scheduled);
    EXPECT_EQ(tm.release(10.0), 1);  // una crítica liberada
    EXPECT_EQ(tm.openCount(), 2);
}

TEST(Tareas, EntregaATiempoYTardia) {
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Low, 0, 50, {1, 1}), makeTask(1, Priority::Low, 0, 50, {1, 1})});
    tm.release(0);
    tm.markPicked(0, 10);
    EXPECT_TRUE(tm.markDelivered(0, 40));
    tm.markPicked(1, 45);
    EXPECT_FALSE(tm.markDelivered(1, 60));
    EXPECT_TRUE(tm.allDelivered());
}

TEST(Tareas, CriticaVencidaSeReportaUnaSolaVez) {
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Critical, 0, 20, {1, 1})});
    tm.release(0);
    EXPECT_EQ(tm.collectNewCriticalOverdue(10), 0);
    EXPECT_EQ(tm.collectNewCriticalOverdue(25), 1);
    EXPECT_EQ(tm.collectNewCriticalOverdue(30), 0);
}

TEST(Tareas, CargaCaidaVuelveAPendiente) {
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Low, 0, 1e9, {1, 1})});
    tm.release(0);
    tm.markPicked(0, 5);
    tm.markDropped(0);
    EXPECT_EQ(tm.get(0).status, TaskStatus::Pending);
    EXPECT_EQ(tm.get(0).incidents, 1);
}

TEST(Selector, PrefiereLaTareaCritica) {
    const auto cfg = test::smallScenario();
    TaskSelector sel;
    sel.configure(cfg);
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Low, 0, 1e9, {3, 3}), makeTask(1, Priority::Critical, 0, 1e9, {12, 3})});
    tm.release(0);
    WarehouseMap m;
    m.width = 16;
    m.height = 12;
    m.chargers = {{2, 10.5f}};
    TaskSelector::Context ctx{{2, 2}, 0.0, cfg.energy.capacityWh, 50.0f, -1};
    const auto d = sel.decide(ctx, tm, m);
    EXPECT_EQ(d.phase, Phase::ToPickup);
    EXPECT_EQ(d.taskId, 1);
}

TEST(Selector, VaARecargarSiNoAlcanzaLaEnergia) {
    const auto cfg = test::smallScenario();
    TaskSelector sel;
    sel.configure(cfg);
    TaskManager tm;
    tm.setTasks({makeTask(0, Priority::Low, 0, 1e9, {14, 3})});
    tm.release(0);
    WarehouseMap m;
    m.width = 16;
    m.height = 12;
    m.chargers = {{2, 10.5f}};
    TaskSelector::Context ctx{{2, 2}, 0.0, 0.01f, 50.0f, -1};
    EXPECT_EQ(sel.decide(ctx, tm, m).phase, Phase::ToCharger);
}
