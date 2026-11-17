// Entrenamiento de SAC con escenarios generados y dificultad adaptativa.
//
// En lugar de etapas fijas (que hacían olvidar lo aprendido al pasar a la
// siguiente: la memoria de repetición solo guarda los últimos minutos), cada
// episodio de cada entorno usa un escenario nuevo del generador. La dificultad
// de cada dimensión sube o baja según el éxito en la frontera, y los niveles ya
// dominados se siguen repasando siempre. Cada cierto tiempo se valida la
// política determinista en E1–E5 con semillas fijas y se guarda la mejor.
//
// Optimizaciones para CPU de 6 núcleos + GPU pequeña:
//  - N entornos en paralelo sobre un grupo fijo de hilos (VecEnv).
//  - Memoria de repetición en la GPU; una sola copia CPU→GPU por paso vectorizado,
//    desde memoria fijada (pinned) y asíncrona.
//  - Inferencia del actor en lote para todos los entornos.
//  - Las actualizaciones se encolan en la GPU de forma asíncrona y la CPU avanza
//    los entornos mientras tanto; la única sincronización por paso es la copia de
//    las acciones de vuelta a la CPU.
//  - Críticos gemelos fusionados (baddbmm), Polyak con operaciones foreach y TF32.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>

#include <torch/cuda.h>

#include "cli.hpp"
#include "warehouse/agent/replay_buffer.hpp"
#include "warehouse/agent/sac.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/env/vec_env.hpp"
#include "warehouse/io/metrics_logger.hpp"
#include "warehouse/sim/scenario_generator.hpp"

using namespace wrl;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

std::atomic<bool> gStop{false};
void onSignal(int) { gStop = true; }

template <typename T>
T value(const Json& j, const char* key, T def) {
    return j.contains(key) ? j[key].get<T>() : def;
}

void usage() {
    std::puts(
        "Uso: wrl_train [--config configs/agent/sac.json] [--out runs/nombre] [--steps N] [--envs N]\n"
        "               [--threads N] [--device auto|cuda|cpu] [--resume modelo.pt]\n"
        "               [--scenario archivo.json]   entrenar en un escenario fijo, sin generador\n"
        "               [--validate-every N]        transiciones entre validaciones\n"
        "               [--blocking-wait]           esperar a la GPU sin ocupar un núcleo (~2.6 % más lento)");
}

/// Puntuación de un episodio para el currículo: fracción entregada, rebajada si
/// terminó por un fallo o con choques dañinos.
double curriculumScore(const EpisodeMetrics& m) {
    double s = m.completionRate();
    if (m.reason == EndReason::WorkerCollision || m.reason == EndReason::CollisionLimit || m.reason == EndReason::Stalled ||
        m.batteryDepleted)
        s *= 0.5;
    if (m.harmfulCollisions() > 0) s *= 0.8;
    return s;
}

Json levelsJson(const Difficulty& d) {
    Json j = Json::object();
    for (int k = 0; k < kSkills; ++k) j[toString(static_cast<Skill>(k))] = d.v[static_cast<size_t>(k)];
    return j;
}

Difficulty levelsFromJson(const Json& j) {
    Difficulty d;
    for (int k = 0; k < kSkills; ++k) d.v[static_cast<size_t>(k)] = j.value(toString(static_cast<Skill>(k)), 0.0f);
    return d;
}

/// Validación con semillas fijas: la política determinista en escenarios de
/// referencia. Es lo que decide qué modelo se guarda como el mejor, en lugar de
/// la tasa del propio entrenamiento (que mezcla dificultades y exploración).
struct Validation {
    std::vector<ScenarioConfig> scenarios;
    int perScenario = 4;
    int threads = 6;

    struct Result {
        std::vector<MetricsSummary> perScenario;
        double score = 0.0;  ///< media de la tasa de completadas (cada escenario pesa igual)
        double onTime = 0.0;
    };

    Result run(SacAgent& agent) const {
        const int n = static_cast<int>(scenarios.size()) * perScenario;
        VecEnv vec(scenarios.front(), n, threads, 0x7A11DA7Eull);  // semillas fijas en cada validación
        vec.setScenarioProvider([this](int i, uint64_t) { return scenarios[static_cast<size_t>(i / perScenario)]; });
        const int obsDim = vec.observationSize();
        std::vector<uint8_t> done(static_cast<size_t>(n), 0);
        std::vector<EpisodeMetrics> results(static_cast<size_t>(n));
        torch::Tensor obs = torch::empty({n, obsDim});
        int remaining = n;
        while (remaining > 0) {
            std::memcpy(obs.data_ptr<float>(), vec.observations(), sizeof(float) * static_cast<size_t>(n * obsDim));
            const auto act = agent.act(obs.to(agent.device()), true).to(torch::kCPU).contiguous();
            vec.step(act.data_ptr<float>());
            const auto& fin = vec.finishedEnvs();
            for (size_t k = 0; k < fin.size(); ++k) {
                const auto i = static_cast<size_t>(fin[k]);
                if (done[i]) continue;
                done[i] = 1;
                results[i] = vec.finishedEpisodes()[k];
                --remaining;
            }
        }
        Result r;
        for (size_t s = 0; s < scenarios.size(); ++s) {
            std::vector<EpisodeMetrics> eps(results.begin() + static_cast<std::ptrdiff_t>(s * perScenario),
                                            results.begin() + static_cast<std::ptrdiff_t>((s + 1) * perScenario));
            r.perScenario.push_back(MetricsSummary::from(eps));
            r.score += r.perScenario.back().completionRate / static_cast<double>(scenarios.size());
            r.onTime += r.perScenario.back().onTimeRate / static_cast<double>(scenarios.size());
        }
        return r;
    }
};

}  // namespace

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("help")) {
        usage();
        return 0;
    }
    const fs::path configPath = args.get("config", std::string(WRL_SOURCE_DIR) + "/configs/agent/sac.json");
    const fs::path configDir = configPath.parent_path();
    const Json cfg = loadJsonWithBase(configPath);
    const Json tj = cfg.value("training", Json::object());
    const Json gj = cfg.value("generator", Json::object());
    const Json vj = cfg.value("validation", Json::object());
    const SacConfig sac = SacConfig::fromJson(cfg.value("sac", Json::object()));

    const int numEnvs = args.getInt("envs", value(tj, "numEnvs", 24));
    const int threads = args.getInt("threads", value(tj, "threads", 6));
    const long totalSteps = static_cast<long>(args.getDouble("steps", value(tj, "totalSteps", 3'000'000.0)));
    const double utd = value(tj, "updatesPerTransition", 0.25);
    const int capacity = value(tj, "replayCapacity", 1'000'000);
    const long warmup = value(tj, "warmupSteps", 20000L);
    const std::string warmupPolicy = value(tj, "warmupPolicy", std::string("dwa"));
    const float warmupNoise = value(tj, "warmupNoise", 0.3f);
    const long logEvery = value(tj, "logEvery", 10000L);
    const long checkpointEvery = value(tj, "checkpointEvery", 200000L);
    const uint64_t seed = value(tj, "seed", 1ull);
    // Antes de tocar CUDA. Por defecto CUDA espera girando (ocupa un núcleo, ~2.6 %
    // más rápido); --blocking-wait duerme el hilo y deja ese núcleo libre.
    const bool blockingSync = args.has("blocking-wait") && preferBlockingCudaSync();
    const torch::Device device = resolveDevice(args.get("device", value(tj, "device", std::string("auto"))));

    const fs::path out = args.get("out", "runs/sac_" + std::to_string(std::time(nullptr)));
    fs::create_directories(out);
    std::ofstream(out / "config.json") << cfg.dump(2);

    // Hilos: los de simulación ocupan los núcleos físicos; Torch usa pocos para no competir.
    torch::manual_seed(seed);
    if (device.is_cuda()) {
        torch::set_num_threads(1);
        at::globalContext().setFloat32MatmulPrecision("high");  // TF32 en Ampere (RTX 30xx)
    } else {
        const int hw = static_cast<int>(std::thread::hardware_concurrency());
        torch::set_num_threads(std::max(1, hw / 2 - threads / 2));
    }

    // --- Escenarios: generador con currículo adaptativo, o uno fijo con --scenario.
    const bool fixed = args.has("scenario");
    const ScenarioGenerator generator(loadScenario(configDir / value(gj, "base", std::string("../robot/default.json"))));
    AdaptiveCurriculum::Params cp;
    cp.frontierProb = value(gj, "frontierProb", cp.frontierProb);
    cp.jointProb = value(gj, "jointProb", cp.jointProb);
    cp.frontierWidth = value(gj, "frontierWidth", cp.frontierWidth);
    cp.step = value(gj, "step", cp.step);
    cp.raiseAt = value(gj, "raiseAt", cp.raiseAt);
    cp.lowerAt = value(gj, "lowerAt", cp.lowerAt);
    cp.window = value(gj, "window", cp.window);
    cp.startLevel = value(gj, "startLevel", cp.startLevel);
    AdaptiveCurriculum curriculum(cp);
    if (args.has("resume")) {
        // Al reanudar se recuperan los niveles alcanzados, si se guardaron junto al modelo.
        const fs::path levelsFile = fs::path(args.get("resume")).parent_path() / "curriculo.json";
        if (fs::exists(levelsFile)) {
            std::ifstream in(levelsFile);
            curriculum.setLevels(levelsFromJson(Json::parse(in)));
        }
    }

    const ScenarioConfig firstScenario = fixed ? loadScenario(args.get("scenario")) : generator.base();
    VecEnv vec(firstScenario, numEnvs, threads, seed);
    const int N = vec.size();
    // Lo sorteado para el episodio en curso de cada entorno y para el siguiente
    // (el entorno se reinicia dentro de step(), antes de que se lea el resultado).
    std::vector<AdaptiveCurriculum::Sample> current(static_cast<size_t>(N)), upcoming(static_cast<size_t>(N));
    if (!fixed) {
        vec.setScenarioProvider([&](int i, uint64_t s) {
            Rng rng(mixSeed(s, 0xC0441C0ull));
            const auto sample = curriculum.sample(rng);
            upcoming[static_cast<size_t>(i)] = sample;
            return generator.generate(sample.difficulty, s);
        });
        current = upcoming;
    }
    const int obsDim = vec.observationSize();

    Validation validation;
    validation.threads = threads;
    validation.perScenario = value(vj, "episodesPerScenario", 4);
    const long validateEvery = static_cast<long>(args.getDouble("validate-every", value(vj, "every", 300000.0)));
    for (const auto& s : vj.value("scenarios", Json::array()))
        validation.scenarios.push_back(loadScenario(configDir / s.get<std::string>()));

    SacAgent agent(obsDim, WarehouseEnv::kActionSize, sac, device);
    if (args.has("resume")) {
        agent.load(args.get("resume"));
        std::printf("Reanudando desde %s\n", args.get("resume").c_str());
    }
    ReplayBuffer replay(capacity, obsDim, WarehouseEnv::kActionSize, device);

    std::printf("Dispositivo: %s | entornos: %d en %d hilos | observación: %d | memoria de repetición: %.0f MB\n",
                device.is_cuda() ? "CUDA" : "CPU", N, vec.threads(), obsDim, static_cast<double>(replay.bytes()) / 1e6);
    if (device.is_cuda())
        std::printf("GPU: TF32 %s | espera a la GPU %s\n", sac.tf32 ? "activado" : "desactivado",
                    blockingSync ? "sin ocupar un núcleo" : "activa (ocupa un núcleo; --blocking-wait lo libera)");
    if (fixed) std::printf("Escenario fijo: %s\n", firstScenario.name.c_str());
    else std::printf("Escenarios generados; niveles iniciales: %s\n", curriculum.levels().str().c_str());

    // Buffers de intercambio CPU↔GPU en memoria fijada (copias asíncronas).
    const auto pinned = torch::TensorOptions().dtype(torch::kFloat32).pinned_memory(device.is_cuda());
    torch::Tensor obsHost = torch::empty({N, obsDim}, pinned);
    torch::Tensor nextHost[2] = {torch::empty({N, obsDim}, pinned), torch::empty({N, obsDim}, pinned)};
    torch::Tensor rewHost[2] = {torch::empty({N}, pinned), torch::empty({N}, pinned)};
    torch::Tensor doneHost[2] = {torch::empty({N}, pinned), torch::empty({N}, pinned)};
    torch::Tensor actHost = torch::empty({N, 2}, pinned);

    std::vector<std::unique_ptr<DwaPolicy>> demo;
    for (int i = 0; i < N; ++i) demo.push_back(std::make_unique<DwaPolicy>());
    Rng noise(seed ^ 0xABCDEF);
    std::vector<Rng> envNoise;
    for (int i = 0; i < N; ++i) envNoise.emplace_back(mixSeed(seed, static_cast<uint64_t>(i) + 77));

    std::ofstream log(out / "train_log.csv");
    log << "transiciones,nivel_medio";
    for (int k = 0; k < kSkills; ++k) log << ",nivel_" << toString(static_cast<Skill>(k));
    log << ",episodios,retorno_medio,tasa_completadas,colisiones_por_ep,incidentes_carga,"
           "perdida_critico,perdida_actor,alfa,entropia,transiciones_por_s,actualizaciones_por_s\n";
    std::ofstream vlog(out / "validacion.csv");
    vlog << "transiciones,puntaje,a_tiempo";
    for (const auto& s : validation.scenarios) vlog << ',' << s.name;
    vlog << ",nivel_medio,mejor\n";

    auto saveLevels = [&] { std::ofstream(out / "curriculo.json") << levelsJson(curriculum.levels()).dump(2); };

    std::signal(SIGINT, onSignal);
    long steps = 0, updates = 0, lastLog = 0, lastCkpt = 0, lastVal = 0, updatesAtLog = 0;
    double updateDebt = 0.0;
    double bestScore = -1.0, bestOnTime = -1.0;
    std::vector<EpisodeMetrics> sinceLog;
    SacLosses last{};
    auto tLog = Clock::now();
    const auto tStart = Clock::now();
    int parity = 0;

    auto validate = [&] {
        if (validation.scenarios.empty()) return;
        const auto t0 = Clock::now();
        const auto r = validation.run(agent);
        const bool best = r.score > bestScore + 1e-9 || (std::abs(r.score - bestScore) < 1e-9 && r.onTime > bestOnTime);
        if (best) {
            bestScore = r.score;
            bestOnTime = r.onTime;
            agent.save(out / "mejor_validacion.pt");
        }
        std::printf("   validación: %.1f%% completadas, %.1f%% a tiempo (", 100 * r.score, 100 * r.onTime);
        vlog << steps << ',' << r.score << ',' << r.onTime;
        for (size_t s = 0; s < r.perScenario.size(); ++s) {
            std::printf("%s%s %.0f%%", s ? ", " : "", validation.scenarios[s].name.c_str(), 100 * r.perScenario[s].completionRate);
            vlog << ',' << r.perScenario[s].completionRate;
        }
        vlog << ',' << curriculum.progress() << ',' << (best ? 1 : 0) << '\n';
        vlog.flush();
        std::printf(") %s[%.0f s]\n", best ? "-> nuevo mejor modelo " : "", std::chrono::duration<double>(Clock::now() - t0).count());
    };

    // Desglose del tiempo por iteración (para saber qué limita la velocidad).
    double tAct = 0, tUpd = 0, tSim = 0, tRest = 0;
    auto lap = [](Clock::time_point& t) {
        const auto now = Clock::now();
        const double d = std::chrono::duration<double>(now - t).count();
        t = now;
        return d;
    };

    while (steps < totalSteps && !gStop) {
        auto tp = Clock::now();
        // 1) Acciones. En la fase de calentamiento se usan demostraciones (DWA con ruido).
        std::memcpy(obsHost.data_ptr<float>(), vec.observations(), sizeof(float) * static_cast<size_t>(N * obsDim));
        const auto obsDev = obsHost.to(device, /*non_blocking=*/true);
        torch::Tensor actDev;
        if (steps < warmup) {
            float* a = actHost.data_ptr<float>();
            if (warmupPolicy == "dwa") {
                vec.parallelFor([&](int i) {
                    const Action act = demo[static_cast<size_t>(i)]->act(vec.env(i));
                    auto& r = envNoise[static_cast<size_t>(i)];
                    a[2 * i] = std::clamp(act.linear + r.normal(0.0f, warmupNoise), -1.0f, 1.0f);
                    a[2 * i + 1] = std::clamp(act.angular + r.normal(0.0f, warmupNoise), -1.0f, 1.0f);
                });
            } else {
                for (int k = 0; k < 2 * N; ++k) a[k] = noise.uniform(-1.0f, 1.0f);
            }
            actDev = actHost.to(device, true);
            if (device.is_cuda()) torch::cuda::synchronize();  // los buffers fijados se reutilizan
        } else {
            actDev = agent.act(obsDev, false);
            actHost.copy_(actDev);  // única sincronización del paso
        }
        tAct += lap(tp);

        // 2) Actualizaciones encoladas en la GPU (asíncronas).
        if (replay.size() >= std::max<long>(sac.batchSize, std::min<long>(warmup, 5000))) {
            updateDebt += utd * N;
            while (updateDebt >= 1.0) {
                last = agent.updateFromReplay(replay);
                updateDebt -= 1.0;
                ++updates;
            }
        }

        tUpd += lap(tp);

        // 3) Simulación en la CPU mientras la GPU trabaja.
        vec.step(actHost.data_ptr<float>());
        tSim += lap(tp);

        // 4) Transiciones a la memoria de repetición. Para los entornos que
        // terminaron, la observación siguiente es la final (antes del reinicio).
        float* next = nextHost[parity].data_ptr<float>();
        std::memcpy(next, vec.observations(), sizeof(float) * static_cast<size_t>(N * obsDim));
        float* rew = rewHost[parity].data_ptr<float>();
        float* done = doneHost[parity].data_ptr<float>();
        for (int i = 0; i < N; ++i) {
            rew[i] = vec.rewards()[i];
            done[i] = vec.terminated()[i] ? 1.0f : 0.0f;  // truncado por tiempo: sí hay bootstrap
            if (vec.terminated()[i] || vec.truncated()[i])
                std::memcpy(next + static_cast<size_t>(i * obsDim), vec.finalObservations() + static_cast<size_t>(i * obsDim),
                            sizeof(float) * static_cast<size_t>(obsDim));
        }
        replay.add(obsDev, actDev, rewHost[parity].to(device, true), nextHost[parity].to(device, true),
                   doneHost[parity].to(device, true));
        parity ^= 1;
        steps += N;

        // 5) Currículo: el resultado de cada episodio terminado ajusta los niveles.
        for (size_t k = 0; k < vec.finishedEpisodes().size(); ++k) {
            const auto& m = vec.finishedEpisodes()[k];
            sinceLog.push_back(m);
            if (fixed) continue;
            const auto i = static_cast<size_t>(vec.finishedEnvs()[k]);
            if (steps > warmup) curriculum.report(current[i], curriculumScore(m));  // las demostraciones no cuentan
            current[i] = upcoming[i];
            demo[i] = std::make_unique<DwaPolicy>();
        }

        tRest += lap(tp);

        if (steps - lastLog >= logEvery) {
            const double dt = std::chrono::duration<double>(Clock::now() - tLog).count();
            const auto s = MetricsSummary::from(sinceLog);
            const bool haveLoss = last.critic.defined();
            const double lc = haveLoss ? last.critic.item<double>() : 0.0;
            const double la = haveLoss ? last.actor.item<double>() : 0.0;
            const double al = haveLoss ? last.alphaValue.item<double>() : 0.0;
            const double en = haveLoss ? last.entropy.item<double>() : 0.0;
            const double sps = static_cast<double>(steps - lastLog) / dt;
            const double ups = static_cast<double>(updates - updatesAtLog) / dt;
            std::printf("[%8ld] nivel %.2f | ep %4d | retorno %8.1f | completadas %5.1f%% | colis/ep %.2f | Lc %.3f La %.2f "
                        "alfa %.3f H %.2f | %.0f trans/s %.0f act/s\n",
                        steps, curriculum.progress(), s.episodes, s.meanReturn, 100 * s.completionRate,
                        s.harmfulCollisionsPerEpisode, lc, la, al, en, sps, ups);
            // Acciones: incluye esperar a que la GPU termine las actualizaciones encoladas.
            const double tot = std::max(1e-9, tAct + tUpd + tSim + tRest);
            std::printf("           tiempo: acciones+espera GPU %.0f%% | encolar actualizaciones %.0f%% | simulación %.0f%% | "
                        "memoria y resto %.0f%%\n",
                        100 * tAct / tot, 100 * tUpd / tot, 100 * tSim / tot, 100 * tRest / tot);
            tAct = tUpd = tSim = tRest = 0;
            log << steps << ',' << curriculum.progress();
            for (float lv : curriculum.levels().v) log << ',' << lv;
            log << ',' << s.episodes << ',' << s.meanReturn << ',' << s.completionRate << ',' << s.harmfulCollisionsPerEpisode
                << ',' << s.loadIncidentRate << ',' << lc << ',' << la << ',' << al << ',' << en << ',' << sps << ',' << ups
                << '\n';
            log.flush();
            sinceLog.clear();
            lastLog = steps;
            updatesAtLog = updates;
            tLog = Clock::now();
        }
        if (steps - lastCkpt >= checkpointEvery) {
            agent.save(out / "checkpoint.pt");
            saveLevels();
            std::printf("   niveles: %s\n", curriculum.levels().str().c_str());
            lastCkpt = steps;
        }
        if (steps > warmup && steps - lastVal >= validateEvery) {
            validate();
            lastVal = steps;
        }
    }

    agent.save(out / "sac_final.pt");
    saveLevels();
    validate();
    const double minutes = std::chrono::duration<double>(Clock::now() - tStart).count() / 60.0;
    std::printf("%s tras %ld transiciones (%.1f min). Modelos: %s (final), %s (mejor validación, %.1f%%)\n",
                gStop ? "Interrumpido" : "Terminado", steps, minutes, (out / "sac_final.pt").string().c_str(),
                (out / "mejor_validacion.pt").string().c_str(), 100 * bestScore);
    return 0;
}
