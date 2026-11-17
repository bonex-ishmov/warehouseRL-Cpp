#include <filesystem>

#include <gtest/gtest.h>

#include "warehouse/agent/replay_buffer.hpp"
#include "warehouse/agent/sac.hpp"

using namespace wrl;

TEST(SAC, AccionesAcotadasYLogProbFinita) {
    torch::manual_seed(0);
    Actor actor(10, 2, 32);
    const auto obs = torch::randn({64, 10});
    auto [a, logp] = actor->sample(obs);
    EXPECT_EQ(a.sizes(), (std::vector<int64_t>{64, 2}));
    EXPECT_EQ(logp.sizes(), (std::vector<int64_t>{64, 1}));
    EXPECT_LE(a.abs().max().item<float>(), 1.0f);
    EXPECT_TRUE(torch::isfinite(logp).all().item<bool>());
}

TEST(SAC, CriticoGemeloDevuelveDosEstimaciones) {
    Critic critic(10, 2, 32);
    const auto q = critic->forward(torch::randn({16, 10}), torch::rand({16, 2}));
    EXPECT_EQ(q.sizes(), (std::vector<int64_t>{2, 16, 1}));
    EXPECT_FALSE(torch::allclose(q[0], q[1]));  // pesos independientes
}

TEST(SAC, AdamForeachMinimizaUnaCuadratica) {
    auto x = torch::full({3}, 5.0f).set_requires_grad(true);
    ForeachAdam opt({x}, 0.1f);
    for (int i = 0; i < 300; ++i) {
        opt.zeroGrad();
        (x - 2.0f).pow(2).sum().backward();
        opt.step();
    }
    EXPECT_TRUE(torch::allclose(x.detach(), torch::full({3}, 2.0f), 1e-2, 1e-2));
}

TEST(SAC, AprendeUnaTareaDeBandido) {
    // Recompensa = -(a - 0.5)^2: la política determinista debe acercarse a 0.5.
    torch::manual_seed(1);
    SacConfig cfg;
    cfg.hidden = 64;
    cfg.batchSize = 128;
    cfg.gamma = 0.0f;
    cfg.rewardScale = 1.0f;
    SacAgent agent(4, 2, cfg, torch::kCPU);
    ReplayBuffer rb(5000, 4, 2, torch::kCPU);
    const auto obs = torch::zeros({256, 4});
    for (int it = 0; it < 15; ++it) {
        const auto a = agent.act(obs, false);
        const auto r = -(a - 0.5f).pow(2).sum(1);
        rb.add(obs, a, r, obs, torch::ones({256}));
        for (int k = 0; k < 40; ++k) agent.update(rb.sample(cfg.batchSize));
    }
    const auto best = agent.act(obs.slice(0, 0, 1), true);
    EXPECT_NEAR(best[0][0].item<float>(), 0.5f, 0.15f);
    EXPECT_NEAR(best[0][1].item<float>(), 0.5f, 0.15f);
}

TEST(SAC, GuardarYCargarConservaLaPolitica) {
    torch::manual_seed(2);
    SacConfig cfg;
    cfg.hidden = 32;
    SacAgent agent(6, 2, cfg, torch::kCPU);
    const auto path = std::filesystem::temp_directory_path() / "wrl_sac_prueba.pt";
    agent.save(path);
    const auto obs = torch::randn({5, 6});
    const auto a1 = agent.act(obs, true);
    Actor loaded = loadActor(path, torch::kCPU);
    const auto a2 = loaded->deterministic(obs);
    EXPECT_TRUE(torch::allclose(a1, a2));
    std::filesystem::remove(path);
}

TEST(ReplayBuffer, CircularYMuestreoDentroDeRango) {
    ReplayBuffer rb(10, 3, 2, torch::kCPU);
    for (int i = 0; i < 4; ++i) {
        const auto v = torch::full({4, 3}, static_cast<float>(i));
        rb.add(v, torch::zeros({4, 2}), torch::full({4}, static_cast<float>(i)), v, torch::zeros({4}));
    }
    EXPECT_EQ(rb.size(), 10);
    const auto b = rb.sampleGraphSafe(256);
    // Solo quedan los lotes 1..3 (el 0 se sobreescribió parcialmente).
    EXPECT_GE(b.rew.min().item<float>(), 0.0f);
    EXPECT_LE(b.rew.max().item<float>(), 3.0f);
    EXPECT_TRUE(torch::equal(b.obs.select(1, 0).view({-1, 1}), b.rew));
}
