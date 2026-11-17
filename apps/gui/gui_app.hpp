#pragma once

// Interfaz gráfica: solo lee el estado de la sesión (wrl::Session) y le envía
// órdenes. La simulación, las políticas y el entrenamiento no saben que existe.

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "warehouse/render/renderer.hpp"
#include "warehouse/session/session.hpp"
#include "warehouse/sim/scenario_generator.hpp"

namespace wrl::gui {

class GuiApp {
public:
    GuiApp(sf::RenderWindow& window, Session& session, std::filesystem::path root);

    void processEvent(const sf::Event& event);
    /// Construye la interfaz del cuadro y dibuja las vistas de los agentes.
    void frame();
    /// Pestaña del panel de métricas al abrir: 0 en vivo, 1 gráficas, 2 resumen.
    void selectMetricsTab(int tab) { initialTab_ = tab; }

    static constexpr size_t kMaxAgents = 6;

private:
    /// Vista del almacén de un agente, dibujada en su propia textura.
    struct Tile {
        std::unique_ptr<sf::RenderTexture> texture;
        std::unique_ptr<Renderer> renderer;
    };

    void drawMenuBar();
    void drawControlPanel(float x, float y, float w, float h);
    void drawViewports(float x, float y, float w, float h);
    void drawMetricsPanel(float x, float y, float w, float h);

    void scenarioSection();
    void generatorSection();
    void agentsSection();
    void playbackSection();
    void viewSection();
    void agentDetails(size_t i);
    void plotsSection();
    void summarySection();
    void timelineSection();
    void legendSection();

    void drawTile(size_t i, float w, float h);
    void handleKey(sf::Keyboard::Key key);
    void updateManualControl();
    void selectScenario(int index);
    bool hasManualAgent() const;
    void refreshModels();

    sf::Color agentColor(size_t i) const;

    sf::RenderWindow& window_;
    Session& session_;
    std::filesystem::path root_;

    std::vector<CatalogEntry> scenarios_;
    std::unique_ptr<ScenarioGenerator> generator_;
    Difficulty genDifficulty_ = Difficulty::uniform(0.5f);
    bool genEachEpisode_ = true;
    std::vector<CatalogEntry> models_;
    int scenarioIndex_ = -1;
    int newKind_ = 0;
    int newModel_ = 0;
    std::string message_;  ///< último error o aviso para el usuario
    uint64_t seedInput_ = 0;
    uint64_t shownSeed_ = 0;  ///< semilla de la sesión la última vez que se copió al campo

    RenderOptions view_;
    std::vector<Tile> tiles_;
    bool showHelp_ = false;
    int initialTab_ = -1;
};

}  // namespace wrl::gui
