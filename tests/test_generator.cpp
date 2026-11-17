#include <algorithm>
#include <filesystem>

#include <gtest/gtest.h>

#include "warehouse/env/policies.hpp"
#include "warehouse/env/vec_env.hpp"
#include "warehouse/sim/scenario_generator.hpp"

using namespace wrl;
namespace fs = std::filesystem;

namespace {

ScenarioGenerator makeGenerator() { return ScenarioGenerator(loadScenario(fs::path(WRL_SOURCE_DIR) / "configs/robot/default.json")); }

Difficulty randomDifficulty(Rng& rng) {
    Difficulty d;
    for (auto& x : d.v) x = rng.uniform();
    return d;
}

}  // namespace

TEST(Generador, EsDeterministaPorSemilla) {
    const auto gen = makeGenerator();
    const auto d = Difficulty::uniform(0.6f);
    const auto a = gen.generate(d, 42), b = gen.generate(d, 42), c = gen.generate(d, 43);
    EXPECT_EQ(a.map.proc.width, b.map.proc.width);
    EXPECT_EQ(a.tasks.count, b.tasks.count);
    EXPECT_EQ(a.energy.capacityWh, b.energy.capacityWh);
    EXPECT_TRUE(a.map.proc.width != c.map.proc.width || a.tasks.count != c.tasks.count ||
                a.energy.capacityWh != c.energy.capacityWh);
}

TEST(Generador, CumpleLasReglasEnTodoElRango) {
    const auto gen = makeGenerator();
    Rng rng(5);
    for (int i = 0; i < 500; ++i) {
        const auto d = randomDifficulty(rng);
        const ScenarioConfig c = gen.generate(d, static_cast<uint64_t>(i));  // validate() se llama dentro
        const auto& p = c.map.proc;
        SCOPED_TRACE(d.str());
        EXPECT_GE(p.aisleWidth, 2.0f * c.sim.inflation + 1.2f - 1e-4f);
        EXPECT_GE(p.crossAisle, 2.0f * c.sim.inflation + 1.2f - 1e-4f);
        EXPECT_GE(c.energy.capacityWh, gen.minimumCapacityWh(p.width, p.height) - 1e-4f);
        EXPECT_GT(c.energy.initialFraction, c.energy.reserveFraction);
        EXPECT_LE(c.obstacles.maxCount, static_cast<int>(p.width * p.height / 80.0f));
        EXPECT_GE(c.tasks.slackMin, 2.0f - 1e-4f);
        EXPECT_FALSE(c.tasks.loadTypes.empty());
        EXPECT_GE(c.sim.maxTime, 180.0f);
    }
}

TEST(Generador, DificultadCeroEsTrivialYUnoExigente) {
    const auto gen = makeGenerator();
    const auto easy = gen.generate(Difficulty::uniform(0.0f), 1);
    EXPECT_EQ(easy.tasks.count, 1);
    EXPECT_EQ(easy.obstacles.maxCount, 0);
    EXPECT_FALSE(easy.tasks.deadlines);
    EXPECT_EQ(easy.map.proc.frictionZones, 0);
    ASSERT_EQ(easy.tasks.loadTypes.size(), 1u);

    const auto hard = gen.generate(Difficulty::uniform(1.0f), 1);
    EXPECT_GE(hard.tasks.count, 8);
    EXPECT_GE(hard.obstacles.maxCount, 3);
    EXPECT_EQ(hard.tasks.loadTypes.size(), 4u);
    EXPECT_LT(hard.energy.capacityWh, 10.0f);
}

TEST(Generador, LosEscenariosSeConstruyenYLaLineaBaseLosResuelve) {
    const auto gen = makeGenerator();
    Rng rng(11);
    int delivered = 0, total = 0;
    for (int i = 0; i < 12; ++i) {
        Difficulty d = randomDifficulty(rng);
        d[Skill::Tasks] = std::min(d[Skill::Tasks], 0.3f);  // episodios cortos para la prueba
        WarehouseEnv env(gen.generate(d, static_cast<uint64_t>(100 + i)));
        env.reset(static_cast<uint64_t>(100 + i));
        DwaPolicy dwa;
        while (!env.step(dwa.act(env)).done()) {}
        delivered += env.metrics().delivered;
        total += env.metrics().tasksTotal;
    }
    EXPECT_GE(static_cast<double>(delivered) / total, 0.8);
}

TEST(Curriculo, SubeConExitoYNoBajaDeCero) {
    AdaptiveCurriculum::Params p;
    p.window = 10;
    AdaptiveCurriculum cur(p);
    Rng rng(3);
    for (int i = 0; i < 3000; ++i) cur.report(cur.sample(rng), 1.0);
    for (float lv : cur.levels().v) EXPECT_GT(lv, 0.3f);

    AdaptiveCurriculum bad(p);
    for (int i = 0; i < 3000; ++i) bad.report(bad.sample(rng), 0.0);
    for (float lv : bad.levels().v) EXPECT_EQ(lv, 0.0f);
}

TEST(Curriculo, SoloUnaDimensionEnLaFronteraYElRestoRepasa) {
    AdaptiveCurriculum cur;
    cur.setLevels(Difficulty::uniform(0.5f));
    Rng rng(9);
    int frontier = 0;
    for (int i = 0; i < 2000; ++i) {
        const auto s = cur.sample(rng);
        for (int k = 0; k < kSkills; ++k) {
            const float x = s.difficulty.v[static_cast<size_t>(k)];
            if (k == s.frontier) {
                EXPECT_GE(x, 0.5f);
                EXPECT_LE(x, 0.6f + 1e-5f);
            } else {
                EXPECT_LE(x, 0.5f + 1e-5f);
            }
        }
        frontier += s.frontier >= 0;
    }
    EXPECT_NEAR(frontier / 2000.0, 0.6, 0.05);
}

TEST(Curriculo, HayEpisodiosDificilesEnTodoALaVez) {
    // Con sorteos independientes, que las 6 dimensiones pasen de 0.7 a la vez
    // ocurre ~0.07 % de las veces; los sorteos correlacionados lo hacen habitual.
    AdaptiveCurriculum::Params p;
    p.frontierProb = 0.0f;
    AdaptiveCurriculum cur(p);
    cur.setLevels(Difficulty::uniform(1.0f));
    Rng rng(4);
    int allHard = 0;
    for (int i = 0; i < 4000; ++i) {
        const auto s = cur.sample(rng);
        allHard += *std::min_element(s.difficulty.v.begin(), s.difficulty.v.end()) > 0.7f;
    }
    EXPECT_GT(allHard / 4000.0, 0.05);
}

TEST(VecEnv, ProveedorDeEscenariosPorEpisodio) {
    const auto gen = makeGenerator();
    VecEnv vec(gen.base(), 4, 2, 1);
    std::vector<int> calls(4, 0);
    vec.setScenarioProvider([&](int i, uint64_t s) {
        ++calls[static_cast<size_t>(i)];
        Difficulty d = Difficulty::uniform(0.0f);
        d[Skill::Layout] = 0.25f * static_cast<float>(i);
        return gen.generate(d, s);
    });
    for (int i = 0; i < 4; ++i) EXPECT_EQ(calls[static_cast<size_t>(i)], 1);
    // Cada entorno tiene su propio mapa.
    EXPECT_NE(vec.env(0).map().width * 100 + vec.env(0).map().height, vec.env(3).map().width * 100 + vec.env(3).map().height);
    std::vector<float> actions(8, 0.0f);
    int finished = 0;
    for (int t = 0; t < 4000 && finished == 0; ++t) {
        vec.step(actions.data());
        ASSERT_EQ(vec.finishedEnvs().size(), vec.finishedEpisodes().size());
        finished += static_cast<int>(vec.finishedEnvs().size());
    }
    ASSERT_GT(finished, 0);  // quieto, termina por falta de progreso
    int resets = 0;
    for (int c : calls) resets += c - 1;
    EXPECT_EQ(resets, finished);
}
