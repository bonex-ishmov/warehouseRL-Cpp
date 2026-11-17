#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "warehouse/planning/path.hpp"
#include "warehouse/world/occupancy_grid.hpp"

namespace wrl {

/// Planificador global A* (8 vecinos) sobre el mapa de ocupación inflado, con
/// costo extra cerca de obstáculos y suavizado por línea de visión (estilo
/// Theta* a posteriori). Reutiliza sus buffers entre llamadas para no reservar
/// memoria en cada replanificación.
class AStarPlanner {
public:
    explicit AStarPlanner(const OccupancyGrid* grid = nullptr, float clearanceCost = 2.0f, float preferredClearance = 1.0f);

    void setGrid(const OccupancyGrid* grid);
    /// Ruta suavizada desde start hasta goal; vacía si no hay camino.
    std::optional<Path> plan(Vec2 start, Vec2 goal);
    /// Longitud de la ruta más corta (sin costo de holgura), o nullopt.
    std::optional<float> shortestLength(Vec2 start, Vec2 goal);

    int lastExpanded() const { return expanded_; }

private:
    std::optional<std::vector<Cell>> search(Cell start, Cell goal, float clearanceCost);
    std::vector<Vec2> smooth(const std::vector<Cell>& cells, Vec2 start, Vec2 goal) const;

    const OccupancyGrid* grid_;
    float clearanceCost_;
    float preferred_;
    std::vector<float> g_;
    std::vector<int> parent_;
    std::vector<uint32_t> stamp_;
    std::vector<uint8_t> closed_;
    uint32_t generation_ = 0;
    int expanded_ = 0;
};

}  // namespace wrl
