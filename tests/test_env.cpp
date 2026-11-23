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
    EXPECT_EQ(env.observationSize(), 36 + 26 + 3 * 8);
    EXPECT_EQ(static_cast<int>(env.observation().size()), env.observationSize());
    for (float x : env.observation()) EXPECT_TRUE(std::isfinite(x));
}

// La distancia entre superficies (GJK) debe quedar entre las cotas geométricas que dan
// los círculos circunscrito e inscrito de cada forma, y los obstáculos salen ordenados.
TEST(Sensores, DistanciaEntreSuperficiesYTipoDeObstaculo) {
    const auto cfg = test::repoScenario("e4_dinamico.json");
    WarehouseEnv env(cfg);
    env.reset(77);
    DwaPolicy p;
    const auto& rp = cfg.robot;
    int checked = 0, workers = 0, forklifts = 0;
    for (int i = 0; i < 600 && !env.done(); ++i) {
        env.step(p.act(env));
        float prev = -1.0f;
        for (const auto& o : env.perceived()) {
            const float center = o.relPos.length();
            const DynamicObstacle* src = nullptr;
            for (const auto& d : env.obstacles())
                if ((d.pos - o.worldPos).lengthSq() < 1e-8f) src = &d;
            ASSERT_NE(src, nullptr);
            EXPECT_EQ(o.worker, src->isWorker());
            const float inner = src->isWorker() ? src->halfExtents.x : std::min(src->halfExtents.x, src->halfExtents.y);
            const float outer = src->isWorker() ? src->halfExtents.x : src->halfExtents.length();
            EXPECT_GE(o.gap, 0.0f);
            EXPECT_LE(o.gap, std::max(0.0f, center - inner - rp.halfWidth) + 1e-3f);
            EXPECT_GE(o.gap, center - outer - rp.radius() - 1e-3f);
            EXPECT_GE(o.gap, prev);
            prev = o.gap;
            (o.worker ? workers : forklifts)++;
            ++checked;
        }
        float w = 0.0f, f = 0.0f;
        env.unsafeSpeed(w, f);
        EXPECT_GE(w, 0.0f);
        EXPECT_GE(f, 0.0f);
        if (env.robot().velocity().length() < 1e-3f) {
            EXPECT_LT(w + f, 1e-3f);  // parado no hay exceso que castigar
        }
    }
    EXPECT_GT(checked, 100);
    EXPECT_GT(workers, 0);
    EXPECT_GT(forklifts, 0);
}

// Velocidad insegura contra lo estático: parado no hay exceso; yendo a fondo en
// línea recta contra lo que haya delante, el exceso aparece antes del choque.
TEST(Entorno, VelocidadInseguraContraEstanterias) {
    auto cfg = test::repoScenario("e1_basico.json");
    cfg.shield.enabled = false;  // se trata justamente de chocar
    WarehouseEnv env(cfg);
    env.reset(5);
    EXPECT_LT(env.unsafeStaticSpeed(), 1e-3f);
    float maxBefore = 0.0f;
    bool hit = false;
    for (int i = 0; i < 400 && !env.done(); ++i) {
        const float u = env.unsafeStaticSpeed();
        EXPECT_GE(u, 0.0f);
        env.step(Action{1.0f, 0.0f});
        const auto& c = env.metrics().collisions;
        if (c[0] + c[1] + c[2] > 0) { hit = true; break; }
        maxBefore = std::max(maxBefore, u);
    }
    ASSERT_TRUE(hit);
    EXPECT_GT(maxBefore, 0.3f);
}

// Los modelos anteriores al v11 (77 observaciones) siguen funcionando: la política
// recibe la observación en su formato, con los obstáculos ordenados por centro.
TEST(Entorno, ObservacionCompatibleConModelosAnteriores) {
    SensorParams sp;  // 36 rayos, 3 obstáculos
    const int n = WarehouseEnv::observationSize(sp), legacy = WarehouseEnv::legacyObservationSize(sp);
    ASSERT_EQ(legacy, 36 + 26 + 15);
    std::vector<float> in(static_cast<size_t>(n)), out(static_cast<size_t>(legacy), -9.0f);
    for (int i = 0; i < n; ++i) in[static_cast<size_t>(i)] = 0.001f * static_cast<float>(i);
    const int head = sp.lidarRays + 20, k = WarehouseEnv::kPerObstacle;
    // Obstáculo 0 lejos (centro), 1 cerca, 2 ausente.
    float* o = in.data() + head;
    o[0] = 0.5f, o[1] = 0.0f, o[4] = 1.0f;
    o[k + 0] = 0.1f, o[k + 1] = 0.1f, o[k + 4] = 1.0f;
    for (int j = 0; j < k; ++j) o[2 * k + j] = 0.0f;
    WarehouseEnv::toLegacyObservation(sp, in.data(), out.data());
    for (int i = 0; i < head; ++i) EXPECT_EQ(out[static_cast<size_t>(i)], in[static_cast<size_t>(i)]);
    for (int j = 0; j < 5; ++j) {
        EXPECT_EQ(out[static_cast<size_t>(head + j)], o[k + j]);      // primero el más cercano
        EXPECT_EQ(out[static_cast<size_t>(head + 5 + j)], o[j]);
        EXPECT_EQ(out[static_cast<size_t>(head + 10 + j)], 0.0f);     // ausente al final
    }
    for (int j = 0; j < 6; ++j) EXPECT_EQ(out[static_cast<size_t>(legacy - 6 + j)], in[static_cast<size_t>(n - 6 + j)]);
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
    cfg.shield.enabled = false;
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

// Con la capa de seguridad, la misma consigna a fondo contra la estantería frena a
// tiempo: ningún contacto y el robot queda parado cerca del margen.
TEST(CapaSeguridad, FrenaAntesDeLaEstanteria) {
    auto cfg = test::smallScenario();
    cfg.map.robotStart = Pose{{3.0f, 5.0f}, 0.0f};
    cfg.shield.enabled = true;
    WarehouseEnv env(cfg);
    env.reset(1);
    int contacts = 0, limited = 0;
    for (int i = 0; i < 60; ++i) {
        const auto r = env.step({1.0f, 0.0f});
        contacts += r.events.collisions[0] + r.events.collisions[1] + r.events.collisions[2];
        limited += env.shieldActive() ? 1 : 0;
    }
    EXPECT_EQ(contacts, 0);
    EXPECT_GT(limited, 0);
    const float front = env.robot().position().x + cfg.robot.halfLength;
    EXPECT_LT(front, 6.0f);
    EXPECT_GT(front, 6.0f - 0.4f);  // no se queda exageradamente lejos
    EXPECT_LT(std::abs(env.robot().forwardSpeed()), 0.05f);
    EXPECT_GT(env.metrics().shieldTime, 0.0);
}

// Al entrenar con la capa, lo que ella recorta cuesta: misma consigna contra la estantería,
// menos recompensa con shieldInterventionPerSecond < 0.
TEST(CapaSeguridad, RecortarLaConsignaCuesta) {
    auto run = [](float perSecond) {
        auto cfg = test::smallScenario();
        cfg.map.robotStart = Pose{{3.0f, 5.0f}, 0.0f};
        cfg.shield.enabled = true;
        cfg.reward.shieldInterventionPerSecond = perSecond;
        WarehouseEnv env(cfg);
        env.reset(1);
        double total = 0.0;
        for (int i = 0; i < 60; ++i) total += env.step({1.0f, 0.0f}).reward;
        return total;
    };
    EXPECT_LT(run(-2.0f), run(0.0f) - 1.0);
}

// Sin nada cerca la capa no toca la consigna, y la velocidad permitida crece con el hueco.
TEST(CapaSeguridad, NoIntervieneEnEspacioLibre) {
    EXPECT_FLOAT_EQ(SafetyShield::allowedClosing(0.0f, 1.0f, 0.1f), 0.0f);
    EXPECT_LT(SafetyShield::allowedClosing(0.3f, 1.0f, 0.1f), SafetyShield::allowedClosing(1.0f, 1.0f, 0.1f));
    // v·T + v²/(2a) = hueco
    const float v = SafetyShield::allowedClosing(1.0f, 1.0f, 0.1f);
    EXPECT_NEAR(v * 0.1f + v * v / 2.0f, 1.0f, 1e-4f);

    auto cfg = test::smallScenario();
    cfg.map.robotStart = Pose{{3.0f, 5.0f}, 0.0f};
    cfg.shield.enabled = true;
    WarehouseEnv env(cfg);
    env.reset(1);
    env.step({0.2f, 0.0f});  // a 2.7 m de la estantería y despacio: nada que recortar
    EXPECT_FALSE(env.shieldActive());
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
