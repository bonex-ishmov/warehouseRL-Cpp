// Visor: conducción manual, política (dwa, random, sac) o reproducción de una traza.

#include <cstdio>
#include <iostream>

#include "cli.hpp"
#include "warehouse/env/policies.hpp"
#include "warehouse/io/trace.hpp"
#include "warehouse/render/renderer.hpp"
#ifdef WRL_HAS_TORCH
#include "warehouse/agent/sac_policy.hpp"
#endif

using namespace wrl;

namespace {

const char* kHelp =
    "Controles: Espacio pausa | N paso | +/- velocidad | R reiniciar | L LiDAR | P ruta | C seguir robot | "
    "rueda zoom | Esc salir\n"
    "Modo manual: W/S o flechas avanzar/retroceder, A/D o flechas girar";

std::string fmt(const char* f, double a, double b = 0, double c = 0) {
    char buf[160];
    std::snprintf(buf, sizeof buf, f, a, b, c);
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("help")) {
        std::puts(
            "Uso: wrl_viewer --scenario archivo.json [--policy manual|dwa|random|sac] [--model modelo.pt] [--seed N]\n"
            "     wrl_viewer --replay traza.json\n"
            "     --shield on|off  capa de seguridad (por defecto, la del escenario: activada)");
        std::puts(kHelp);
        return 0;
    }

    std::optional<Trace> trace;
    ScenarioConfig cfg;
    std::string mode = args.get("policy", "manual");
    uint64_t seed = static_cast<uint64_t>(args.getInt("seed", 1));
    if (args.has("replay")) {
        trace = Trace::load(args.get("replay"));
        cfg = parseScenario(trace->scenario);
        seed = trace->seed;
        mode = "traza (" + trace->policy + ")";
    } else {
        cfg = loadScenario(args.get("scenario", std::string(WRL_SOURCE_DIR) + "/configs/scenarios/demo_mapa_fijo.json"));
        if (args.has("shield")) cfg.shield.enabled = args.get("shield") != "off";
    }

    std::unique_ptr<Policy> policy;
    if (!trace && mode != "manual") {
#ifdef WRL_HAS_TORCH
        if (mode == "sac") policy = makeSacPolicy(args.get("model", "models/sac_final.pt"), "cpu");
        else policy = makeBuiltinPolicy(mode);
#else
        policy = makeBuiltinPolicy(mode);
#endif
    }

    WarehouseEnv env(cfg);
    env.reset(seed);
    if (policy) policy->reset(seed);

    // Captura sin ventana: simula N segundos y guarda una imagen (para documentación).
    if (args.has("screenshot")) {
        const double until = args.getDouble("at", 20.0);
        while (!env.done() && env.time() < until) {
            Action a{};
            if (trace) {
                if (trace->actions.empty()) break;
                a = trace->actions.front();
                trace->actions.erase(trace->actions.begin());
            } else if (policy) {
                a = policy->act(env);
            }
            env.step(a);
        }
        sf::RenderTexture target({1280u, 860u});
        Renderer r(target);
        target.clear(sf::Color(20, 22, 26));
        r.draw(env, RenderOptions{});
        r.drawHud({std::string("WarehouseRL — ") + cfg.name + " | " + mode,
                   fmt("t = %.1f s   entregadas %.0f/%.0f", env.time(), env.metrics().delivered, env.metrics().tasksTotal),
                   std::string("fase: ") + toString(env.decision().phase)});
        target.display();
        const auto out = args.get("screenshot");
        if (!target.getTexture().copyToImage().saveToFile(out)) return 1;
        std::printf("Imagen guardada en %s\n", out.c_str());
        return 0;
    }

    sf::RenderWindow window(sf::VideoMode({1280u, 860u}), "WarehouseRL");
    window.setFramerateLimit(60);
    Renderer renderer(window);
    if (!renderer.hasFont()) std::puts("Aviso: no se encontró una fuente del sistema; el panel no mostrará texto.");
    std::puts(kHelp);

    RenderOptions opt;
    bool paused = false, stepOnce = false;
    float speed = 1.0f;
    size_t traceIdx = 0;
    double accumulator = 0.0;
    StepResult lastResult;
    sf::Clock clock;
    using Key = sf::Keyboard::Key;

    auto restart = [&](uint64_t s) {
        env.reset(s);
        if (policy) policy->reset(s);
        traceIdx = 0;
        lastResult = {};
    };

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* k = event->getIf<sf::Event::KeyPressed>()) {
                switch (k->code) {
                    case Key::Escape: window.close(); break;
                    case Key::Space: paused = !paused; break;
                    case Key::N: stepOnce = true; break;
                    case Key::Equal:
                    case Key::Add: speed = std::min(32.0f, speed * 2.0f); break;
                    case Key::Hyphen:
                    case Key::Subtract: speed = std::max(0.125f, speed * 0.5f); break;
                    case Key::R: restart(trace ? seed : ++seed); break;
                    case Key::L: opt.lidar = !opt.lidar; break;
                    case Key::P: opt.path = !opt.path; break;
                    case Key::C: opt.follow = !opt.follow; break;
                    default: break;
                }
            } else if (const auto* w = event->getIf<sf::Event::MouseWheelScrolled>()) {
                opt.zoom = std::clamp(opt.zoom * (w->delta > 0 ? 1.15f : 1.0f / 1.15f), 0.5f, 6.0f);
            } else if (const auto* r = event->getIf<sf::Event::Resized>()) {
                (void)r;
            }
        }

        // Avance en tiempo simulado proporcional al real.
        accumulator += clock.restart().asSeconds() * speed;
        int budget = 64;
        while ((!paused && accumulator >= env.controlDt() && budget-- > 0) || stepOnce) {
            accumulator = std::max(0.0, accumulator - env.controlDt());
            stepOnce = false;
            if (env.done()) break;
            Action a;
            if (trace) {
                if (traceIdx >= trace->actions.size()) break;
                a = trace->actions[traceIdx++];
            } else if (policy) {
                a = policy->act(env);
            } else {
                const bool up = sf::Keyboard::isKeyPressed(Key::W) || sf::Keyboard::isKeyPressed(Key::Up);
                const bool down = sf::Keyboard::isKeyPressed(Key::S) || sf::Keyboard::isKeyPressed(Key::Down);
                const bool left = sf::Keyboard::isKeyPressed(Key::A) || sf::Keyboard::isKeyPressed(Key::Left);
                const bool right = sf::Keyboard::isKeyPressed(Key::D) || sf::Keyboard::isKeyPressed(Key::Right);
                a.linear = up ? 1.0f : down ? -1.0f : 0.0f;
                a.angular = left ? 0.8f : right ? -0.8f : 0.0f;
            }
            lastResult = env.step(a);
        }
        if (paused) accumulator = 0.0;

        const auto& m = env.metrics();
        const auto& d = env.decision();
        std::vector<std::string> hud = {
            std::string("WarehouseRL — ") + cfg.name + " | modo: " + mode,
            fmt("t = %.1f s   velocidad x%.3g   semilla %.0f", env.time(), speed, static_cast<double>(env.seed())),
            std::string("fase: ") + toString(d.phase) + (d.taskId >= 0 ? "  tarea " + std::to_string(d.taskId) : ""),
            fmt("batería %.0f%%   carga %.0f kg   fricción %.2f", 100.0 * env.batteryFraction(), env.robot().payload(),
                env.frictionUnderRobot()),
            fmt("v %.2f m/s   w %.2f rad/s   derrape %.2f m/s", env.robot().forwardSpeed(), env.robot().angularSpeed(),
                env.robot().lateralSpeed()),
            fmt("entregadas %.0f/%.0f   a tiempo %.0f", m.delivered, m.tasksTotal, m.onTime),
            fmt("roces %.0f  leves %.0f  graves %.0f", m.collisions[0], m.collisions[1], m.collisions[2]),
            fmt("incidentes de carga %.0f   recargas %.0f   energía %.2f Wh", m.loadIncidents, m.recharges, m.energyWh),
            fmt("retorno %.1f   recompensa %.2f", m.totalReturn, lastResult.reward),
        };
        if (env.done()) hud.push_back(std::string("FIN: ") + toString(m.reason) + "  (R para reiniciar)");
        if (paused) hud.push_back("PAUSA");

        window.clear(sf::Color(20, 22, 26));
        renderer.draw(env, opt);
        renderer.drawHud(hud);
        window.display();
    }
    return 0;
}
