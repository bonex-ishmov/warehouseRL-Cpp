#pragma once

#include <vector>

#include "warehouse/core/math.hpp"
#include "warehouse/core/rng.hpp"
#include "warehouse/io/config.hpp"

namespace wrl {

/// Geometría estática de un almacén ya instanciado (explícito o procedural).
struct WarehouseMap {
    float width = 0.0f;
    float height = 0.0f;
    std::vector<Rect> walls;
    std::vector<Rect> shelves;
    std::vector<Rect> restricted;
    std::vector<FrictionZone> frictionZones;
    std::vector<Vec2> pickups;
    std::vector<Vec2> dropoffs;
    std::vector<Vec2> chargers;
    Pose robotStart;

    /// Coeficiente de adherencia del suelo en un punto.
    float frictionAt(Vec2 p, float nominal) const;
    bool inRestricted(Vec2 p) const;
    bool insideSolid(Vec2 p, float margin = 0.0f) const;
};

/// Construye el mapa a partir de la especificación. Si es procedural usa rng.
WarehouseMap buildMap(const MapSpec& spec, Rng& rng);
WarehouseMap generateProcedural(const ProceduralParams& p, Rng& rng);

}  // namespace wrl
