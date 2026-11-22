#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include "warehouse/session/session.hpp"

using namespace wrl;
namespace fs = std::filesystem;

namespace {

Session makeSession(int tasks = 1) {
    Session s([](const AgentSpec& spec) { return makeBuiltinPolicy(spec.kind); });
    s.setScenario(test::smallScenario(tasks));
    s.setAutoAdvance(false);
    return s;
}

void runToEnd(Session& s, int maxSteps = 20000) {
    for (int i = 0; i < maxSteps && !s.allFinished(); ++i) s.stepAll();
}

}  // namespace

TEST(Sesion, AgentesIgualesRecorrenElMismoEpisodio) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    s.addAgent({"dwa", "", ""});
    s.reset(5);
    for (int i = 0; i < 200; ++i) s.stepAll();
    const Vec2 a = s.agent(0).env().robot().position(), b = s.agent(1).env().robot().position();
    EXPECT_FLOAT_EQ(a.x, b.x);
    EXPECT_FLOAT_EQ(a.y, b.y);
    EXPECT_EQ(s.agent(0).history().size(), s.agent(1).history().size());
    EXPECT_EQ(s.agent(0).spec().label, "A* + DWA");
}

TEST(Sesion, ElEpisodioTerminadoSeRegistraUnaVez) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    runToEnd(s);
    ASSERT_TRUE(s.allFinished());
    for (int i = 0; i < 10; ++i) s.stepAll();  // pasos extra no registran de nuevo
    ASSERT_EQ(s.agent(0).records().size(), 1u);
    const auto stats = summarize(s.agent(0).records());
    EXPECT_EQ(stats.episodes, 1);
    EXPECT_DOUBLE_EQ(stats.completion, s.agent(0).env().metrics().completionRate());
}

TEST(Sesion, AvanceSegunTiempoRealYPausa) {
    Session s = makeSession();
    s.addAgent({"random", "", ""});
    const double dt = s.agent(0).env().controlDt();
    // Cuadros de 1/4 de paso: a x1 hacen falta 4 para un paso de control.
    auto frames = [&](int n) {
        int steps = 0;
        for (int i = 0; i < n; ++i) steps += s.advance(dt / 4 + 1e-7);
        return steps;
    };
    s.setSpeed(1.0);
    EXPECT_EQ(frames(40), 10);
    s.setSpeed(4.0);
    EXPECT_EQ(frames(5), 5);
    // Un cuadro muy largo (ventana arrastrada, depurador) no dispara una avalancha de pasos.
    s.setSpeed(1.0);
    EXPECT_LE(s.advance(30.0), static_cast<int>(0.25 / dt) + 1);
    s.setPaused(true);
    EXPECT_EQ(s.advance(1.0), 0);
    s.requestStep();
    EXPECT_EQ(s.advance(0.0), 1);
}

TEST(Sesion, AvanzaSoloAlSiguienteEpisodio) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    s.reset(3);
    runToEnd(s);
    s.setAutoAdvance(true);
    EXPECT_EQ(s.advance(0.1), 0);
    EXPECT_EQ(s.seed(), 3u);  // primero se muestra el final un momento
    s.advance(Session::kAutoAdvanceDelay);
    EXPECT_EQ(s.seed(), 4u);
    EXPECT_FALSE(s.allFinished());
}

TEST(Sesion, CambiarEscenarioConservaAgentesYBorraRegistros) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    s.addAgent({"random", "", "otra"});
    runToEnd(s);
    s.setScenario(test::smallScenario(2));
    ASSERT_EQ(s.agentCount(), 2u);
    EXPECT_EQ(s.agent(1).spec().label, "otra");
    EXPECT_TRUE(s.agent(0).records().empty());
    EXPECT_EQ(s.agent(0).env().metrics().tasksTotal, 2);
}

TEST(Sesion, AccionManual) {
    Session s = makeSession();
    s.addAgent({"manual", "", ""});
    const Vec2 start = s.agent(0).env().robot().position();
    s.setManualAction({1.0f, 0.0f});
    for (int i = 0; i < 20; ++i) s.stepAll();
    EXPECT_GT(distance(s.agent(0).env().robot().position(), start), 0.3f);
}

TEST(Sesion, PoliticaInvalidaLanzaExcepcion) {
    Session s = makeSession();
    EXPECT_THROW(s.addAgent({"no_existe", "", ""}), std::runtime_error);
    EXPECT_EQ(s.agentCount(), 0u);
}

TEST(Catalogo, EncuentraEscenariosYEtapas) {
    const auto list = listScenarios(fs::path(WRL_SOURCE_DIR) / "configs/scenarios");
    EXPECT_GE(list.size(), 14u);
    bool train = false;
    for (const auto& e : list) {
        EXPECT_EQ(e.description.rfind("no se pudo", 0), std::string::npos) << e.path;
        train |= e.path.parent_path().filename() == "train";
    }
    EXPECT_TRUE(train);
}

TEST(Sesion, RegistraSucesosYEstados) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    runToEnd(s);
    const auto& h = s.agent(0).history();
    ASSERT_EQ(s.agent(0).env().metrics().reason, EndReason::AllDelivered);
    int pickups = 0, delivered = 0;
    for (const auto& e : h.events) {
        if (e.kind == EventKind::Pickup) {
            ++pickups;
            EXPECT_EQ(e.detail, 0);
        }
        if (e.kind == EventKind::Delivered || e.kind == EventKind::DeliveredLate) {
            ++delivered;
            EXPECT_EQ(e.detail, 0);
        }
    }
    EXPECT_EQ(pickups, 1);
    EXPECT_EQ(delivered, 1);
    // Entre la recogida y la entrega el robot figura con carga.
    EXPECT_TRUE(std::any_of(h.flags.begin(), h.flags.end(), [](uint8_t f) { return f & kCarrying; }));
    EXPECT_EQ(h.flags.size(), h.trail.size());
    EXPECT_EQ(h.loadStress.size(), h.time.size());
}

TEST(Sesion, EscenarioNuevoEnCadaEpisodio) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    int calls = 0;
    s.setScenarioProvider([&](uint64_t seed) {
        ++calls;
        auto cfg = test::smallScenario(1 + static_cast<int>(seed % 3));
        return cfg;
    });
    EXPECT_TRUE(s.hasScenarioProvider());
    s.reset(1);
    EXPECT_EQ(s.agent(0).env().metrics().tasksTotal, 2);
    s.reset(2);
    EXPECT_EQ(s.agent(0).env().metrics().tasksTotal, 3);
    EXPECT_GE(calls, 3);
    s.setScenario(test::smallScenario(1));
    EXPECT_FALSE(s.hasScenarioProvider());
}

// El interruptor de la interfaz llega a los entornos de todos los agentes y se mantiene
// al cambiar de escenario.
TEST(Sesion, InterruptorDeLaCapaDeSeguridad) {
    Session s = makeSession();
    s.addAgent({"dwa", "", ""});
    s.addAgent({"dwa", "", ""});
    s.setShield(true);
    EXPECT_TRUE(s.shieldEnabled());
    for (size_t i = 0; i < 2; ++i) EXPECT_TRUE(s.agent(i).env().config().shield.enabled);
    s.setShield(false);
    EXPECT_FALSE(s.shieldEnabled());
    EXPECT_FALSE(s.agent(1).env().config().shield.enabled);
    auto cfg = test::smallScenario();
    cfg.shield.enabled = true;
    s.setScenario(cfg);
    EXPECT_FALSE(s.agent(0).env().config().shield.enabled);
}
