// Evaluación determinista (sin exploración) sobre escenarios y semillas
// (sección 10). Exporta un CSV por episodio, un resumen JSON y el veredicto
// de los criterios de aceptación de cada escenario.

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

#include "cli.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/io/metrics_logger.hpp"
#include "warehouse/io/trace.hpp"
#ifdef WRL_HAS_TORCH
#include "warehouse/agent/sac_policy.hpp"
#endif

using namespace wrl;
namespace fs = std::filesystem;

static std::unique_ptr<Policy> createPolicy(const std::string& name, const std::string& model) {
#ifdef WRL_HAS_TORCH
    if (name == "sac") return makeSacPolicy(model, "cpu");
#else
    if (name == "sac") throw std::runtime_error("compilado sin LibTorch: la política sac no está disponible");
#endif
    (void)model;
    return makeBuiltinPolicy(name);
}

static void usage() {
    std::puts(
        "Uso: wrl_evaluate --policy dwa|random|sac [--model models/sac.pt] --scenario archivo.json [--scenario ...]\n"
        "                  [--episodes 100] [--seeds 5] [--threads N] [--out results/eval] [--traces 1]\n"
        "                  [--shield on|off]   capa de seguridad (por defecto, la del escenario: activada)");
}

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("help") || !args.has("scenario")) {
        usage();
        return args.has("help") ? 0 : 1;
    }
    const std::string policyName = args.get("policy", "dwa");
    const std::string model = args.get("model", "models/sac_final.pt");
    const int episodes = args.getInt("episodes", 100);
    const int seeds = std::max(1, args.getInt("seeds", 5));
    const int threads = std::max(1, args.getInt("threads", static_cast<int>(std::thread::hardware_concurrency()) - 1));
    const int tracesPerSeed = args.getInt("traces", 1);
    const fs::path outDir = args.get("out", "results/eval_" + policyName);
    fs::create_directories(outDir);
    if (policyName == "sac" && !fs::exists(model)) {
        std::fprintf(stderr, "No existe el modelo %s\n", model.c_str());
        return 1;
    }

    Json report = Json::object();
    bool allPass = true;
    for (const auto& scenarioPath : args.all("scenario")) {
        ScenarioConfig cfg = loadScenario(scenarioPath);
        if (args.has("shield")) cfg.shield.enabled = args.get("shield") != "off";
        const int perSeed = std::max(1, episodes / seeds);
        std::printf("\n== %s  (%s%s, %d episodios x %d semillas)\n", cfg.name.c_str(), policyName.c_str(),
                    cfg.shield.enabled ? " + capa de seguridad" : "", perSeed, seeds);

        // Semillas de evaluación disjuntas de las de entrenamiento.
        struct Job { int seedIdx; int ep; uint64_t seed; };
        std::vector<Job> jobs;
        for (int s = 0; s < seeds; ++s)
            for (int e = 0; e < perSeed; ++e)
                jobs.push_back({s, e, mixSeed(0xE7A1'0000ull + static_cast<uint64_t>(s) * 7919ull, static_cast<uint64_t>(e))});

        std::vector<EpisodeMetrics> results(jobs.size());
        std::atomic<size_t> next{0};
        std::mutex ioMutex;
        auto worker = [&] {
            WarehouseEnv env(cfg);
            auto policy = createPolicy(policyName, model);
            while (true) {
                const size_t j = next++;
                if (j >= jobs.size()) return;
                const Job& job = jobs[j];
                env.reset(job.seed);
                policy->reset(job.seed);
                const bool trace = job.ep < tracesPerSeed;
                env.setRecordActions(trace);
                StateRecorder states;
                while (true) {
                    const StepResult r = env.step(policy->act(env));
                    if (trace) states.record(env, r);
                    if (r.done()) break;
                }
                results[j] = env.metrics();
                if (trace) {
                    const std::string stem = cfg.name + "_s" + std::to_string(job.seedIdx) + "_e" + std::to_string(job.ep);
                    Trace t{cfg.raw, job.seed, policyName, env.recordedActions()};
                    std::lock_guard lock(ioMutex);
                    t.save(outDir / "traces" / (stem + ".json"));
                    states.save(outDir / "traces" / (stem + ".csv"));
                }
            }
        };
        std::vector<std::jthread> pool;
        for (int t = 0; t < std::min<int>(threads, static_cast<int>(jobs.size())); ++t) pool.emplace_back(worker);
        pool.clear();

        MetricsLogger logger(outDir / (cfg.name + "_episodios.csv"));
        for (size_t j = 0; j < jobs.size(); ++j) logger.log(results[j], cfg.name, policyName, jobs[j].seed);

        // Resumen global y dispersión entre semillas.
        const auto total = MetricsSummary::from(results);
        std::vector<double> perSeedCompletion;
        for (int s = 0; s < seeds; ++s) {
            std::vector<EpisodeMetrics> sub(results.begin() + s * perSeed, results.begin() + (s + 1) * perSeed);
            perSeedCompletion.push_back(MetricsSummary::from(sub).completionRate);
        }
        double mean = 0, var = 0;
        for (double v : perSeedCompletion) mean += v / seeds;
        for (double v : perSeedCompletion) var += (v - mean) * (v - mean) / seeds;

        std::printf("  completadas %.1f%% (±%.1f entre semillas) | a tiempo %.1f%% | sin colisiones %.1f%% | ratio ruta %.2f\n",
                    100 * total.completionRate, 100 * std::sqrt(var), 100 * total.onTimeRate,
                    100 * total.collisionFreeEpisodes, total.pathRatio);
        std::printf("  energía/entrega %.2f Wh | incidentes de carga %.1f%% | batería agotada %.1f%% | retorno %.1f ± %.1f\n",
                    total.energyPerDeliveryWh, 100 * total.loadIncidentRate, 100 * total.depletionRate, total.meanReturn,
                    total.stdReturn);

        Json crit = Json::array();
        for (const auto& c : total.check(cfg.acceptance)) {
            std::printf("  [%s] %s: %.3f (%s %.3f)\n", c.pass ? "CUMPLE" : "NO CUMPLE", c.name.c_str(), c.value,
                        c.lowerIsBetter ? "<=" : ">=", c.threshold);
            allPass &= c.pass;
            crit.push_back({{"criterio", c.name}, {"valor", c.value}, {"umbral", c.threshold}, {"cumple", c.pass}});
        }
        Json sj = total.toJson();
        sj["completadas_desviacion_semillas"] = std::sqrt(var);
        report[cfg.name] = {{"resumen", sj}, {"criterios", crit}, {"politica", policyName}, {"capa_seguridad", cfg.shield.enabled}};
    }

    std::ofstream(outDir / "resumen.json") << report.dump(2);
    std::printf("\nResultados en %s\n", outDir.string().c_str());
    return allPass ? 0 : 2;
}
