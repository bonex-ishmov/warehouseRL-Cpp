// Mide cuánto tarda una actualización de SAC en la GPU, aislada de la
// simulación. En el entrenamiento real la GPU es el cuello de botella (la
// simulación de 24 entornos ocupa ~2 % de cada iteración), así que este número
// fija la velocidad del entrenamiento: transiciones/s ≈ 1 / (ms por
// actualización × actualizaciones por transición).

#include <chrono>
#include <cstdio>

#include <torch/cuda.h>

#include "cli.hpp"
#include "warehouse/agent/replay_buffer.hpp"
#include "warehouse/agent/sac.hpp"

using namespace wrl;
using Clock = std::chrono::steady_clock;

static double measure(SacAgent& agent, ReplayBuffer& replay, int iters) {
    for (int i = 0; i < 20; ++i) agent.updateFromReplay(replay);  // calentamiento (y captura del grafo)
    torch::cuda::synchronize();
    const auto t0 = Clock::now();
    for (int i = 0; i < iters; ++i) agent.updateFromReplay(replay);
    torch::cuda::synchronize();
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count() / iters;
}

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("blocking")) preferBlockingCudaSync();
    if (!torch::cuda::is_available()) {
        std::fprintf(stderr, "Este banco de pruebas necesita CUDA.\n");
        return 1;
    }
    const int obsDim = args.getInt("obs", 86);
    const int iters = args.getInt("iters", 2000);
    const double utd = args.getDouble("utd", 0.25);
    const torch::Device device(torch::kCUDA);

    ReplayBuffer replay(200'000, obsDim, 2, device);
    {
        const auto o = torch::TensorOptions().device(device);
        for (int k = 0; k < 20; ++k)
            replay.add(torch::rand({10'000, obsDim}, o), torch::rand({10'000, 2}, o) * 2 - 1, torch::randn({10'000}, o),
                       torch::rand({10'000, obsDim}, o), torch::zeros({10'000}, o));
    }

    {
        // ¿Se aplica TF32? Con TF32 el producto difiere del FP32 en ~1e-3 relativo.
        const auto a = torch::randn({256, 256}, torch::TensorOptions().device(device));
        at::globalContext().setAllowTF32CuBLAS(false);
        const auto exact = torch::mm(a, a);
        at::globalContext().setAllowTF32CuBLAS(true);
        const auto fast = torch::mm(a, a);
        const auto fastB = torch::bmm(a.unsqueeze(0), a.unsqueeze(0)).squeeze(0);
        std::printf("Diferencia relativa con TF32: mm %.2e, bmm %.2e\n\n",
                    ((fast - exact).abs().max() / exact.abs().max()).item<double>(),
                    ((fastB - exact).abs().max() / exact.abs().max()).item<double>());
    }
    std::printf("Actualización de SAC (obs %d, %d iteraciones, %.2f actualizaciones por transición)\n\n", obsDim, iters, utd);
    std::printf("  lote  oculta  LayerNorm  TF32   ms/act.   act./s   techo trans/s\n");
    struct Case {
        int batch, hidden;
        bool ln, tf32;
    };
    const Case cases[] = {{512, 256, true, false}, {512, 256, true, true},  {512, 256, false, false},
                          {256, 256, true, false}, {1024, 256, true, false}, {1024, 256, true, true}};
    for (const auto& c : cases) {
        SacConfig cfg;
        cfg.tf32 = c.tf32;
        cfg.batchSize = c.batch;
        cfg.hidden = c.hidden;
        cfg.criticLayerNorm = c.ln;
        SacAgent agent(obsDim, 2, cfg, device);
        const double ms = measure(agent, replay, iters);
        std::printf("  %4d  %6d  %9s  %4s  %8.3f  %7.0f  %14.0f\n", c.batch, c.hidden, c.ln ? "sí" : "no", c.tf32 ? "sí" : "no",
                    ms, 1000.0 / ms, 1000.0 / (ms * utd));
    }
    return 0;
}
