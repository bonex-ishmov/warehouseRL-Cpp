// Mide el rendimiento de la simulación sin renderizado (requisito no funcional
// de la sección 5.2) y cómo escala con el número de hilos de VecEnv.

#include <chrono>
#include <cstdio>
#include <thread>

#include "cli.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/env/vec_env.hpp"

using namespace wrl;
using Clock = std::chrono::steady_clock;

static double seconds(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double>(b - a).count(); }

static double runSingle(const ScenarioConfig& cfg, Policy& policy, double budget) {
    WarehouseEnv env(cfg);
    env.reset(1);
    policy.reset(1);
    long steps = 0;
    uint64_t ep = 1;
    const auto t0 = Clock::now();
    while (seconds(t0, Clock::now()) < budget) {
        for (int k = 0; k < 200; ++k) {
            if (env.step(policy.act(env)).done()) env.reset(++ep);
            ++steps;
        }
    }
    return static_cast<double>(steps) / seconds(t0, Clock::now());
}

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    const std::string path = args.get("scenario", std::string(WRL_SOURCE_DIR) + "/configs/scenarios/e5_combinado.json");
    const double budget = args.getDouble("seconds", 3.0);
    const int hw = static_cast<int>(std::thread::hardware_concurrency());
    const int maxThreads = args.getInt("max-threads", hw);
    const int perThread = args.getInt("envs-per-thread", 8);

    const ScenarioConfig cfg = loadScenario(path);
    const int physicsPerAction = cfg.sim.physicsStepsPerAction;
    std::printf("Escenario: %s  (rayos LiDAR: %d, física %d Hz, control %.0f Hz)\n", cfg.name.c_str(), cfg.sensors.lidarRays,
                static_cast<int>(1.0f / cfg.sim.physicsDt + 0.5f), 1.0f / (cfg.sim.physicsDt * physicsPerAction));
    std::printf("Hilos de hardware: %d\n\n", hw);

    RandomPolicy rnd;
    DwaPolicy dwa;
    const double sRand = runSingle(cfg, rnd, budget);
    const double sDwa = runSingle(cfg, dwa, budget);
    std::printf("Un entorno, política aleatoria : %9.0f pasos de control/s  (%9.0f pasos físicos/s)\n", sRand,
                sRand * physicsPerAction);
    std::printf("Un entorno, A* + DWA           : %9.0f pasos de control/s\n", sDwa);
    std::printf("Meta de la propuesta           : %9d pasos de control/s por entorno -> %s\n\n", 2000,
                sRand >= 2000 ? "CUMPLE" : "NO CUMPLE");

    std::printf("Escalado de VecEnv (política aleatoria, %d entornos por hilo):\n", perThread);
    std::printf("  hilos  entornos   pasos/s   aceleración\n");
    double base = 0;
    Rng rng(3);
    for (int t = 1; t <= maxThreads; t = (t < 2 ? t + 1 : t + 2)) {
        const int n = perThread * t;
        VecEnv vec(cfg, n, t, 99);
        std::vector<float> actions(static_cast<size_t>(2 * n));
        long steps = 0;
        const auto t0 = Clock::now();
        while (seconds(t0, Clock::now()) < budget) {
            for (auto& a : actions) a = rng.uniform(-1.0f, 1.0f);
            vec.step(actions.data());
            steps += n;
        }
        const double sps = static_cast<double>(steps) / seconds(t0, Clock::now());
        if (t == 1) base = sps;
        std::printf("  %5d  %8d  %9.0f   %6.2fx\n", t, n, sps, sps / base);
    }
    return 0;
}
