// Valida escenarios antes de entrenar (riesgo "plazos imposibles", sección 11):
// carga y comprueba la configuración, genera mapas con varias semillas,
// verifica que todo sea alcanzable y ejecuta la línea base A* + DWA.
//
// Con --generator N valida el generador de escenarios: N escenarios por nivel
// de dificultad (0, 0.25 … 1 en todas las dimensiones, más mezclas al azar).

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>

#include "cli.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/io/metrics_logger.hpp"
#include "warehouse/sim/scenario_generator.hpp"

using namespace wrl;
namespace fs = std::filesystem;

namespace {

int validateGenerator(int perLevel, int threads) {
    const ScenarioGenerator gen(loadScenario(fs::path(WRL_SOURCE_DIR) / "configs/robot/default.json"));
    struct Job {
        std::string level;
        Difficulty d;
        uint64_t seed;
    };
    std::vector<Job> jobs;
    const char* names[] = {"0.00", "0.25", "0.50", "0.75", "1.00"};
    Rng rng(99);
    for (int l = 0; l < 5; ++l)
        for (int i = 0; i < perLevel; ++i) jobs.push_back({names[l], Difficulty::uniform(0.25f * l), mixSeed(7, jobs.size())});
    for (int i = 0; i < perLevel; ++i) {
        Difficulty d;
        for (auto& x : d.v) x = rng.uniform();
        jobs.push_back({"mezcla", d, mixSeed(7, jobs.size())});
    }
    // Una dimensión al máximo y el resto en 0.3: muestra cuál complica más.
    for (int k = 0; k < kSkills; ++k)
        for (int i = 0; i < perLevel; ++i) {
            Difficulty d = Difficulty::uniform(0.3f);
            d.v[static_cast<size_t>(k)] = 1.0f;
            jobs.push_back({std::string("solo_") + toString(static_cast<Skill>(k)), d, mixSeed(7, jobs.size())});
        }
    // Todo al máximo salvo una dimensión: muestra qué combinación complica.
    for (int k = 0; k < kSkills; ++k)
        for (int i = 0; i < perLevel; ++i) {
            Difficulty d = Difficulty::uniform(1.0f);
            d.v[static_cast<size_t>(k)] = 0.3f;
            jobs.push_back({std::string("sin_") + toString(static_cast<Skill>(k)), d, mixSeed(7, jobs.size())});
        }

    struct Result {
        EpisodeMetrics m;
        std::string error;
    };
    std::vector<Result> results(jobs.size());
    std::atomic<size_t> next{0};
    {
        std::vector<std::jthread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&] {
                for (size_t k; (k = next++) < jobs.size();) {
                    try {
                        WarehouseEnv env(gen.generate(jobs[k].d, jobs[k].seed));
                        env.reset(jobs[k].seed);
                        DwaPolicy policy;
                        while (!env.step(policy.act(env)).done()) {}
                        results[k].m = env.metrics();
                    } catch (const std::exception& e) {
                        results[k].error = e.what();
                    }
                }
            });
    }

    int errors = 0, weak = 0;
    std::printf("Generador: %zu escenarios resueltos con A* + DWA\n\n", jobs.size());
    std::printf("%-16s %5s %9s %9s %9s %9s %9s %8s\n", "nivel", "ep.", "complet.", "a_tiempo", "sin_colis", "bateria0", "atascos", "errores");
    for (const char* level : {"0.00", "0.25", "0.50", "0.75", "1.00", "mezcla", "solo_mapa", "solo_tareas", "solo_energia",
                              "solo_cargas", "solo_obstaculos", "solo_terreno", "sin_mapa", "sin_tareas", "sin_energia",
                              "sin_cargas", "sin_obstaculos", "sin_terreno"}) {
        std::vector<EpisodeMetrics> eps;
        int err = 0, depleted = 0, stalled = 0;
        for (size_t k = 0; k < jobs.size(); ++k) {
            if (jobs[k].level != level) continue;
            if (!results[k].error.empty()) {
                ++err;
                continue;
            }
            eps.push_back(results[k].m);
            depleted += results[k].m.batteryDepleted;
            stalled += results[k].m.reason == EndReason::Stalled;
        }
        const auto s = MetricsSummary::from(eps);
        errors += err;
        std::printf("%-16s %5zu %8.0f%% %8.0f%% %8.0f%% %9d %9d %8d\n", level, eps.size(), 100 * s.completionRate,
                    100 * s.onTimeRate, 100 * s.collisionFreeEpisodes, depleted, stalled, err);
    }
    std::printf("\nEscenarios donde la línea base entrega menos de la mitad:\n");
    for (size_t k = 0; k < jobs.size(); ++k) {
        if (!results[k].error.empty()) {
            std::printf("  ERROR semilla %llu [%s]: %s\n", static_cast<unsigned long long>(jobs[k].seed), jobs[k].d.str().c_str(),
                        results[k].error.c_str());
            continue;
        }
        const auto& m = results[k].m;
        if (m.completionRate() >= 0.5) continue;
        ++weak;
        std::printf("  semilla %llu [%s]: %d/%d, %s\n", static_cast<unsigned long long>(jobs[k].seed), jobs[k].d.str().c_str(),
                    m.delivered, m.tasksTotal, toString(m.reason));
    }
    if (weak == 0) std::printf("  ninguno\n");
    std::printf("\n%d errores, %d escenarios débiles de %zu\n", errors, weak, jobs.size());
    return errors ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("generator"))
        return validateGenerator(args.getInt("generator", 20),
                                 args.getInt("threads", std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2)));
    const int episodes = args.getInt("episodes", 5);
    std::vector<fs::path> files;
    for (const auto& p : args.positional()) files.emplace_back(p);
    if (files.empty()) {
        for (const auto& e : fs::directory_iterator(fs::path(WRL_SOURCE_DIR) / "configs/scenarios"))
            if (e.path().extension() == ".json") files.push_back(e.path());
        std::sort(files.begin(), files.end());
    }

    int failures = 0;
    std::printf("%-22s %8s %9s %9s %9s %9s %8s\n", "escenario", "pickups", "complet.", "a_tiempo", "sin_colis", "ratio", "estado");
    for (const auto& f : files) {
        try {
            const ScenarioConfig cfg = loadScenario(f);
            WarehouseEnv env(cfg);
            DwaPolicy policy;
            std::vector<EpisodeMetrics> eps;
            size_t pickups = 0;
            for (int e = 0; e < episodes; ++e) {
                env.reset(mixSeed(424242, static_cast<uint64_t>(e)));
                pickups += env.map().pickups.size();
                while (!env.step(policy.act(env)).done()) {}
                eps.push_back(env.metrics());
            }
            const auto s = MetricsSummary::from(eps);
            const bool ok = s.completionRate >= 0.5;
            failures += ok ? 0 : 1;
            std::printf("%-22s %8.1f %8.0f%% %8.0f%% %8.0f%% %9.2f %8s\n", cfg.name.c_str(),
                        static_cast<double>(pickups) / episodes, 100 * s.completionRate, 100 * s.onTimeRate,
                        100 * s.collisionFreeEpisodes, s.pathRatio, ok ? "ok" : "REVISAR");
        } catch (const std::exception& e) {
            ++failures;
            std::printf("%-22s ERROR: %s\n", f.filename().string().c_str(), e.what());
        }
    }
    if (failures) std::printf("\n%d escenario(s) requieren revisión.\n", failures);
    return failures ? 1 : 0;
}
