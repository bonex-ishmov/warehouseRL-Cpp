// Interfaz gráfica para ver y comparar agentes ya entrenados en los escenarios.
//
// Uso: wrl_gui [--scenario archivo.json] [--agent sac:modelo.pt] [--agent dwa] [--seed N]
//              [--generated dificultad] [--speed x] [--max-speed] [--tab 0|1|2] [--size 1600x950]
//              [--screenshot imagen.png --frames N]
//
// Sin --agent se abre con el modelo entregado (models/sac_final.pt) frente a A* + DWA.

#include <cstdio>
#include <filesystem>
#include <iostream>

#include <imgui-SFML.h>
#include <imgui.h>

#include "../cli.hpp"
#include "../policy_factory.hpp"
#include "gui_app.hpp"
#include "warehouse/sim/scenario_generator.hpp"

using namespace wrl;
namespace fs = std::filesystem;

namespace {

AgentSpec parseAgent(const std::string& arg) {
    AgentSpec spec;
    const auto colon = arg.find(':');
    spec.kind = arg.substr(0, colon);
    if (colon != std::string::npos) spec.model = arg.substr(colon + 1);
    return spec;
}

void loadFont() {
    // La fuente por defecto de ImGui no trae tildes ni eñes.
    for (const char* path : {"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                             "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
                             "/usr/share/fonts/TTF/DejaVuSans.ttf", "C:/Windows/Fonts/segoeui.ttf"}) {
        if (!fs::exists(path)) continue;
        ImGuiIO& io = ImGui::GetIO();
        // Latín + griego (μ) + puntuación (…, —) + flechas (→).
        static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0370, 0x03FF, 0x2010, 0x205E, 0x2190, 0x21FF, 0};
        if (io.Fonts->AddFontFromFileTTF(path, 15.0f, nullptr, ranges)) break;
    }
    if (ImGui::GetIO().Fonts->Fonts.empty()) ImGui::GetIO().Fonts->AddFontDefault();
    if (!ImGui::SFML::UpdateFontTexture()) std::cerr << "Aviso: no se pudo crear la textura de la fuente\n";
}

void applyStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.WindowBorderSize = 0.0f;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.11f, 0.125f, 1.0f);
}

}  // namespace

int main(int argc, char** argv) {
    cli::Args args(argc, argv);
    if (args.has("help")) {
        std::puts(
            "Uso: wrl_gui [--scenario archivo.json | --generated 0..1] [--agent tipo[:modelo.pt]]... [--seed N] [--speed x]\n"
            "             [--max-speed] [--tab 0|1|2] [--size ANCHOxALTO] [--screenshot imagen.png --frames N]\n"
            "Tipos: sac, dwa, random, manual. Sin --agent: sac:models/sac_final.pt y dwa.");
        return 0;
    }
    const fs::path root = WRL_SOURCE_DIR;

    Session session(makePolicy);
    try {
        session.setScenario(fs::path(args.get("scenario", (root / "configs/scenarios/e1_basico.json").string())));
    } catch (const std::exception& e) {
        std::cerr << "No se pudo cargar el escenario: " << e.what() << "\n";
        return 1;
    }
    session.reset(static_cast<uint64_t>(args.getInt("seed", 1)));
    std::unique_ptr<ScenarioGenerator> generator;
    if (args.has("generated")) {
        // Escenario aleatorio del generador, uno nuevo por episodio, con la misma dificultad en todo.
        generator = std::make_unique<ScenarioGenerator>(loadScenario(root / "configs/robot/default.json"));
        const Difficulty d = Difficulty::uniform(static_cast<float>(args.getDouble("generated", 0.5)));
        session.setScenarioProvider([g = generator.get(), d](uint64_t s) { return g->generate(d, s); });
    }
    session.setSpeed(args.getDouble("speed", 2.0));
    session.setMaxSpeed(args.has("max-speed"));

    auto agents = args.all("agent");
    if (agents.empty()) {
        if (hasSacSupport() && fs::exists(root / "models/sac_final.pt"))
            agents.push_back("sac:" + (root / "models/sac_final.pt").string());
        agents.push_back("dwa");
    }
    for (const auto& a : agents) {
        try {
            session.addAgent(parseAgent(a));
        } catch (const std::exception& e) {
            std::cerr << "No se pudo crear el agente '" << a << "': " << e.what() << "\n";
        }
    }

    unsigned width = 1600, height = 950;
    if (args.has("size")) std::sscanf(args.get("size").c_str(), "%ux%u", &width, &height);
    sf::RenderWindow window(sf::VideoMode({width, height}), "WarehouseRL - Visualización de agentes");
    window.setVerticalSyncEnabled(true);
    if (!ImGui::SFML::Init(window, false)) {
        std::cerr << "No se pudo iniciar ImGui\n";
        return 1;
    }
    ImGui::GetIO().IniFilename = nullptr;  // el diseño es fijo: no hace falta imgui.ini
    applyStyle();
    loadFont();

    gui::GuiApp app(window, session, root);
    if (args.has("tab")) app.selectMetricsTab(args.getInt("tab", 0));

    // Captura para documentación: dibuja N cuadros y guarda la ventana.
    const bool capture = args.has("screenshot");
    const int captureFrames = args.getInt("frames", 120);
    int frame = 0;

    sf::Clock clock;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto* k = event->getIf<sf::Event::KeyPressed>(); k && k->code == sf::Keyboard::Key::Escape)
                window.close();
            else app.processEvent(*event);
        }
        if (!window.isOpen()) break;

        const sf::Time dt = clock.restart();
        // En la captura el tiempo avanza fijo para que la imagen sea reproducible.
        session.advance(capture ? 1.0 / 60.0 : dt.asSeconds());

        ImGui::SFML::Update(window, dt);
        app.frame();
        window.clear(sf::Color(20, 22, 26));
        ImGui::SFML::Render(window);

        if (capture && ++frame >= captureFrames) {
            sf::Texture shot(window.getSize());
            shot.update(window);
            const auto out = args.get("screenshot");
            if (!shot.copyToImage().saveToFile(out)) return 1;
            std::printf("Imagen guardada en %s\n", out.c_str());
            window.close();
            break;
        }
        window.display();
    }
    ImGui::SFML::Shutdown();
    return 0;
}
