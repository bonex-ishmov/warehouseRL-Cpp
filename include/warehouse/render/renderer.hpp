#pragma once

#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "warehouse/env/warehouse_env.hpp"
#include "warehouse/session/history.hpp"

namespace wrl {

/// Colores y símbolos compartidos por el mapa y la leyenda de la interfaz.
namespace palette {
inline const sf::Color kFloor(38, 41, 46);
inline const sf::Color kShelf(139, 94, 60);
inline const sf::Color kWall(110, 110, 115);
inline const sf::Color kRestricted(220, 60, 60);
inline const sf::Color kFriction(70, 140, 235);
inline const sf::Color kPickup(80, 200, 120);
inline const sf::Color kDropoff(240, 150, 50);
inline const sf::Color kCharger(240, 220, 60);
inline const sf::Color kPath(80, 220, 230);
inline const sf::Color kForklift(230, 200, 40);
inline const sf::Color kWorker(220, 80, 220);
inline const sf::Color kPriority[4] = {sf::Color(170, 170, 170), sf::Color(80, 150, 255), sf::Color(255, 160, 40),
                                       sf::Color(255, 60, 60)};
inline const sf::Color kSeverity[3] = {sf::Color(250, 215, 90), sf::Color(255, 140, 40), sf::Color(255, 50, 50)};
inline const sf::Color kDelivered(70, 200, 100);
inline const sf::Color kLate(230, 170, 40);
inline const sf::Color kPickupEvent(120, 200, 255);
inline const sf::Color kLoadDrop(255, 70, 120);
inline const sf::Color kSlip(255, 70, 70);
/// Fases del selector de tareas (línea de tiempo y etiquetas).
inline const sf::Color kPhase[4] = {sf::Color(90, 150, 255), sf::Color(255, 160, 60), sf::Color(240, 210, 60),
                                    sf::Color(110, 110, 120)};
/// Color de un nivel de estrés de la carga (0 tranquila, 1 se cae).
sf::Color stressColor(float stress);
/// Rampa azul → verde → rojo para el recorrido coloreado por velocidad.
sf::Color speedColor(float fraction);
}  // namespace palette

enum class TrailMode : uint8_t { State, Speed };

struct RenderOptions {
    bool lidar = true;
    bool path = true;
    bool follow = false;
    float zoom = 1.0f;
    Vec2 pan;                             ///< desplazamiento de la cámara (m)
    std::optional<sf::Color> robotColor;  ///< para distinguir agentes al compararlos
    /// Historial del episodio: recorrido y sucesos. Opcional (el visor sencillo no lo usa).
    const EpisodeHistory* history = nullptr;
    bool trail = true;
    TrailMode trailMode = TrailMode::State;
    bool events = true;   ///< marcas de choques, entregas, caídas de carga…
    bool intent = true;   ///< trayectoria que seguiría el robot con la acción actual
    bool labels = true;   ///< textos sobre el mapa (tareas, zonas, sucesos)
};

/// Dibuja el almacén en coordenadas del mundo (metros, eje y hacia arriba) y
/// un panel de información en coordenadas de pantalla.
class Renderer {
public:
    explicit Renderer(sf::RenderTarget& target);

    void draw(const WarehouseEnv& env, const RenderOptions& opt);
    void drawHud(const std::vector<std::string>& lines);
    bool hasFont() const { return font_.has_value(); }

    /// Segundos simulados que una marca de suceso se anima y muestra su texto.
    static constexpr float kRecentEvent = 4.0f;

private:
    struct Label {
        Vec2 world;
        std::string text;
        sf::Color color;
        unsigned size = 12;
        float dyPixels = 0.0f;  ///< desplazamiento vertical en pantalla
        bool bold = false;
    };

    void setWorldView(const WarehouseEnv& env, const RenderOptions& opt);

    // Primitivas acumuladas en un único lote de triángulos (en metros).
    void tri(Vec2 a, Vec2 b, Vec2 c, sf::Color col);
    void quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, sf::Color col);
    void rect(const Rect& r, sf::Color fill);
    void rectOutline(const Rect& r, float w, sf::Color col);
    void box(Vec2 center, Vec2 half, float theta, sf::Color col);
    void disc(Vec2 c, float r, sf::Color col, int segments = 20);
    void ring(Vec2 c, float r, float w, sf::Color col, float a0 = 0.0f, float a1 = 2.0f * kPi, int segments = 28);
    void segment(Vec2 a, Vec2 b, float w, sf::Color col);
    void dashed(const std::vector<Vec2>& pts, float w, float dash, float gap, sf::Color col);
    void arrow(Vec2 from, Vec2 to, float w, float head, sf::Color col);
    void hatch(const Rect& r, float spacing, float w, sf::Color col);
    void bolt(Vec2 c, float s, sf::Color col);
    void flush();

    void drawFloor(const WarehouseEnv& env, const RenderOptions& opt);
    void drawTasks(const WarehouseEnv& env, const RenderOptions& opt);
    void drawTrail(const EpisodeHistory& h, const WarehouseEnv& env, const RenderOptions& opt);
    void drawEvents(const EpisodeHistory& h, const WarehouseEnv& env, const RenderOptions& opt);
    void drawEventIcon(const EventMark& e, float s);
    void drawObstacles(const WarehouseEnv& env);
    void drawRobot(const WarehouseEnv& env, const RenderOptions& opt);
    void drawLabels();
    void drawScaleBar();

    sf::RenderTarget& window_;
    std::optional<sf::Font> font_;
    sf::View view_;
    sf::VertexArray batch_{sf::PrimitiveType::Triangles};
    std::vector<Label> labels_;
    float metersPerPixel_ = 0.02f;
};

}  // namespace wrl
