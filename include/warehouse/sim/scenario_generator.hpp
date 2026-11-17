#pragma once

#include <array>
#include <string>

#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"

namespace wrl {

/// Dimensiones de dificultad que el generador varía por separado.
enum class Skill : uint8_t { Layout, Tasks, Energy, Loads, Obstacles, Terrain };
inline constexpr int kSkills = 6;
const char* toString(Skill s);

/// Nivel de cada dimensión en [0, 1] (0 = trivial, 1 = como E5 o algo más).
struct Difficulty {
    std::array<float, kSkills> v{};

    float& operator[](Skill s) { return v[static_cast<size_t>(s)]; }
    float operator[](Skill s) const { return v[static_cast<size_t>(s)]; }
    static Difficulty uniform(float x) {
        Difficulty d;
        d.v.fill(x);
        return d;
    }
    std::string str() const;
};

/// Genera escenarios aleatorios a partir de reglas, en vez de depender de unos
/// pocos archivos fijos. Las reglas garantizan que cada escenario se puede
/// resolver:
///  - pasillos al menos 1.2 m más anchos que el área inflada del robot;
///  - batería suficiente para el viaje más largo razonable con la carga más
///    pesada, y carga inicial para llegar a un cargador;
///  - plazos con holgura mínima de 2x el tiempo estimado y llegadas no mucho
///    más rápidas que el tiempo de servicio (en lo más difícil hay algo de
///    cola, como en E5, pero no crece sin límite);
///  - obstáculos móviles limitados por la superficie libre (sin atascos permanentes);
///  - tiempo máximo proporcional al trabajo pedido.
/// El resultado es determinista para (dificultad, semilla). Los puntos que
/// queden inalcanzables los descarta luego el entorno al construir el mapa.
class ScenarioGenerator {
public:
    /// base: parámetros comunes (robot, sensores, recompensas…), normalmente configs/robot/default.json.
    explicit ScenarioGenerator(ScenarioConfig base);

    ScenarioConfig generate(const Difficulty& d, uint64_t seed) const;
    const ScenarioConfig& base() const { return base_; }

    /// Capacidad mínima de batería que exigen las reglas para un mapa dado.
    float minimumCapacityWh(float width, float height) const;

private:
    ScenarioConfig base_;
};

/// Currículo automático sin etapas fijas.
///
/// Cada dimensión tiene un nivel "dominado". En cada episodio todas las
/// dimensiones se sortean entre 0 y su nivel (repaso de lo ya aprendido; a
/// veces por separado y a veces juntas, para que haya episodios difíciles en
/// todo a la vez) y, con
/// cierta probabilidad, una sola se lleva a la frontera, un poco por encima de
/// su nivel. El resultado de esos episodios de frontera sube o baja el nivel de
/// esa dimensión. Como lo fácil se sigue viendo siempre, no hay olvido por
/// cambio de etapa.
class AdaptiveCurriculum {
public:
    struct Params {
        float frontierProb = 0.6f;   ///< episodios que empujan una dimensión
        /// Episodios con dificultad correlacionada: todas las dimensiones a una
        /// misma fracción de su nivel. Sin esto, que todas salgan altas a la vez
        /// (como en E5) es rarísimo con sorteos independientes.
        float jointProb = 0.5f;
        float frontierWidth = 0.1f;  ///< cuánto por encima del nivel se prueba
        float step = 0.05f;
        float raiseAt = 0.75f;       ///< éxito medio en la frontera para subir
        float lowerAt = 0.35f;       ///< por debajo, el nivel baja
        int window = 40;             ///< episodios de frontera por decisión
        float startLevel = 0.0f;
    };

    /// Lo que se sorteó para un episodio: la dificultad y qué dimensión se empujó (-1 ninguna).
    struct Sample {
        Difficulty difficulty;
        int frontier = -1;
    };

    AdaptiveCurriculum();
    explicit AdaptiveCurriculum(Params p);

    /// Solo lee el estado: se puede llamar desde los hilos de simulación.
    Sample sample(Rng& rng) const;
    /// Resultado de un episodio en [0, 1]. Llamar desde un solo hilo.
    void report(const Sample& s, double score);

    const Difficulty& levels() const { return levels_; }
    void setLevels(const Difficulty& d) { levels_ = d; }
    /// Suma de los niveles / kSkills: un número para resumir el progreso.
    float progress() const;

private:
    Params p_;
    Difficulty levels_;
    std::array<double, kSkills> sum_{};
    std::array<int, kSkills> count_{};
};

}  // namespace wrl
