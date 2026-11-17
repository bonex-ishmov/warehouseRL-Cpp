#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

/// Mapa de ocupación derivado de la geometría. Solo se usa para planificar
/// rutas: el robot se mueve en el plano continuo, nunca sobre la cuadrícula.
class OccupancyGrid {
public:
    OccupancyGrid() = default;
    OccupancyGrid(const WarehouseMap& map, float resolution, float inflation);

    int width() const { return w_; }
    int height() const { return h_; }
    float resolution() const { return res_; }

    bool inBounds(Cell c) const { return c.x >= 0 && c.y >= 0 && c.x < w_ && c.y < h_; }
    int index(Cell c) const { return c.y * w_ + c.x; }
    Cell toCell(Vec2 p) const;
    Vec2 toWorld(Cell c) const { return {(static_cast<float>(c.x) + 0.5f) * res_, (static_cast<float>(c.y) + 0.5f) * res_}; }

    /// Bloqueada tras inflar los obstáculos con el radio del robot.
    bool blocked(Cell c) const { return !inBounds(c) || clearance_[index(c)] < inflation_; }
    /// Distancia en metros al obstáculo más cercano (sin inflar).
    float clearance(Cell c) const { return inBounds(c) ? clearance_[index(c)] : 0.0f; }
    float clearanceAt(Vec2 p) const { return clearance(toCell(p)); }

    std::optional<Cell> nearestFree(Cell c, int maxRadius = 15) const;
    /// Segmento libre sobre celdas no bloqueadas.
    bool lineFree(Vec2 a, Vec2 b) const;
    /// Celdas alcanzables desde start (para validar escenarios).
    std::vector<uint8_t> reachable(Cell start) const;

private:
    int w_ = 0, h_ = 0;
    float res_ = 0.2f;
    float inflation_ = 0.45f;
    std::vector<float> clearance_;
};

}  // namespace wrl
