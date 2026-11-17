#pragma once

#include <vector>

#include "warehouse/core/math.hpp"

namespace wrl {

/// Polilínea con longitudes acumuladas: permite proyectar el robot sobre la
/// ruta, medir lo que falta y obtener el punto de seguimiento (carrot).
class Path {
public:
    struct Projection {
        float s = 0.0f;        ///< longitud de arco del punto proyectado
        float lateral = 0.0f;  ///< distancia del punto a la ruta
    };

    Path() = default;
    explicit Path(std::vector<Vec2> points);

    bool empty() const { return pts_.size() < 2; }
    float length() const { return cum_.empty() ? 0.0f : cum_.back(); }
    const std::vector<Vec2>& points() const { return pts_; }
    Vec2 goal() const { return pts_.empty() ? Vec2{} : pts_.back(); }

    /// Proyección buscando desde una pista de segmento para que sea O(1) amortizado.
    Projection project(Vec2 p) const;
    Vec2 pointAt(float s) const;
    /// Distancia restante hasta el final, medida a lo largo de la ruta.
    float remaining(Vec2 p) const;

private:
    std::vector<Vec2> pts_;
    std::vector<float> cum_;
};

}  // namespace wrl
