#include <filesystem>

#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/env/vec_env.hpp"
#include "warehouse/io/metrics_logger.hpp"
#include "warehouse/io/trace.hpp"

using namespace wrl;
namespace fs = std::filesystem;

TEST(Configuracion, TodosLosEscenariosCargan) {
    int n = 0;
    for (const auto& dir : {"configs/scenarios", "configs/scenarios/train"}) {
        for (const auto& e : fs::directory_iterator(fs::path(WRL_SOURCE_DIR) / dir)) {
            if (e.path().extension() != ".json") continue;
            EXPECT_NO_THROW(loadScenario(e.path())) << e.path();
            ++n;
        }
    }
    EXPECT_GE(n, 14);
}

TEST(Configuracion, ValoresInvalidosSeRechazan) {
    Json j = loadJsonWithBase(std::string(WRL_SOURCE_DIR) + "/configs/robot/default.json");
    j["tasks"]["count"] = 0;
    EXPECT_THROW(parseScenario(j), std::runtime_error);
}

TEST(Entorno, TamanoDeLaObservacion) {
    WarehouseEnv env(test::smallScenario());
    EXPECT_EQ(env.observationSize(), 36 + 26 + 15);
    EXPECT_EQ(static_cast<int>(env.observation().size()), env.observationSize());
    for (float x : env.observation()) EXPECT_TRUE(std::isfinite(x));
}

TEST(Entorno, EsDeterministaConLaMismaSemilla) {
    const auto cfg = test::repoScenario("e5_combinado.json");
    auto run = [&] {
        WarehouseEnv env(cfg);
        env.reset(1234);
        RandomPolicy p;
        p.reset(9);
        std::vector<float> trace;
        for (int i = 0; i < 300 && !env.done(); ++i) {
            const auto r = env.step(p.act(env));
            trace.push_back(r.reward);
            trace.push_back(env.robot().position().x);
        }
        trace.insert(trace.end(), env.observation().begin(), env.observation().end());
        return trace;
    };
    EXPECT_EQ(run(), run());
}

TEST(Entorno, SemillasDistintasDanMapasDistintos) {
    const auto cfg = test::repoScenario("e1_basico.json");
    WarehouseEnv a(cfg), b(cfg);
    a.reset(1);
    b.reset(2);
    EXPECT_NE(a.map().pickups.size() * 1000 + a.tasks().get(0).pickup.x, b.map().pickups.size() * 1000 + b.tasks().get(0).pickup.x);
}

TEST(Entorno, RecogidaYEntregaConLaLineaBase) {
    WarehouseEnv env(test::smallScenario());
    env.reset(5);
    DwaPolicy dwa;
    int pickups = 0;
    StepResult r;
    while (!(r = env.step(dwa.act(env))).done()) pickups += r.events.pickups;
    EXPECT_EQ(pickups, 1);
    EXPECT_EQ(r.reason, EndReason::AllDelivered);
    EXPECT_EQ(env.metrics().delivered, 1);
    EXPECT_GT(env.metrics().totalReturn, 50.0);
}

TEST(Entorno, AcoplarExigeDetenerse) {
    // Pasar por encima del punto de recogida a toda velocidad no cuenta.
    auto cfg = test::smallScenario();
    cfg.map.robotStart = Pose{{5.0f, 3.2f}, 0.0f};
    WarehouseEnv env(cfg);
    env.reset(1);
    int pickups = 0;
    for (int i = 0; i < 30; ++i) pickups += env.step({1.0f, 0.0f}).events.pickups;
    EXPECT_EQ(pickups, 0);
}

TEST(Entorno, BateriaAgotadaTerminaElEpisodio) {
    auto cfg = test::smallScenario();
    cfg.energy.initialFraction = 0.001f;
    WarehouseEnv env(cfg);
    env.reset(1);
    StepResult r;
    for (int i = 0; i < 200 && !r.done(); ++i) r = env.step({1.0f, 0.0f});
    EXPECT_TRUE(r.terminated);
    EXPECT_EQ(r.reason, EndReason::BatteryDepleted);
}

TEST(Entorno, ChoqueContraUnMuroSeDetecta) {
    auto cfg = test::smallScenario();
    cfg.map.robotStart = Pose{{3.0f, 5.0f}, 0.0f};  // de frente a la estantería (x = 6)
    WarehouseEnv env(cfg);
    env.reset(1);
    int harmful = 0;
    for (int i = 0; i < 40; ++i) {
        const auto r = env.step({1.0f, 0.0f});
        harmful += r.events.collisions[1] + r.events.collisions[2];
    }
    EXPECT_GE(harmful, 1);
    EXPECT_LT(env.robot().position().x, 6.0f);
}

TEST(Entorno, TrazaReproduceElEpisodio) {
    const auto cfg = test::repoScenario("e4_dinamico.json");
    WarehouseEnv env(cfg);
    env.setRecordActions(true);
    env.reset(77);
    DwaPolicy dwa;
    for (int i = 0; i < 400 && !env.done(); ++i) env.step(dwa.act(env));
    const Trace t{cfg.raw, 77, "dwa", env.recordedActions()};
    const auto path = fs::temp_directory_path() / "wrl_traza_prueba.json";
    t.save(path);
    const Trace loaded = Trace::load(path);

    WarehouseEnv replay(parseScenario(loaded.scenario));
    replay.reset(loaded.seed);
    for (const auto& a : loaded.actions) replay.step(a);
    EXPECT_FLOAT_EQ(replay.robot().position().x, env.robot().position().x);
    EXPECT_FLOAT_EQ(replay.robot().position().y, env.robot().position().y);
    EXPECT_EQ(replay.metrics().delivered, env.metrics().delivered);
    fs::remove(path);
}

TEST(VecEnv, ElParalelismoNoCambiaLosResultados) {
    const auto cfg = test::repoScenario("e4_dinamico.json");
    auto run = [&](int threads) {
        VecEnv vec(cfg, 8, threads, 5);
        std::vector<float> actions(16);
        Rng rng(3);
        std::vector<float> out;
        for (int s = 0; s < 200; ++s) {
            for (auto& a : actions) a = rng.uniform(-1.0f, 1.0f);
            vec.step(actions.data());
            out.insert(out.end(), vec.rewards(), vec.rewards() + 8);
        }
        out.insert(out.end(), vec.observations(), vec.observations() + 8 * vec.observationSize());
        return out;
    };
    EXPECT_EQ(run(1), run(4));
}

TEST(Metricas, CriteriosDeAceptacion) {
    EpisodeMetrics a, b;
    a.tasksTotal = b.tasksTotal = 10;
    a.delivered = 10;
    a.onTime = 9;
    b.delivered = 8;
    b.onTime = 6;
    b.collisions[1] = 1;
    const auto s = MetricsSummary::from({a, b});
    EXPECT_NEAR(s.completionRate, 0.9, 1e-9);
    EXPECT_NEAR(s.collisionFreeEpisodes, 0.5, 1e-9);
    const auto c = s.check(Json{{"minCompletionRate", 0.9}, {"minOnTimeRate", 0.8}});
    ASSERT_EQ(c.size(), 2u);
    EXPECT_TRUE(c[0].pass);
    EXPECT_FALSE(c[1].pass);
}
