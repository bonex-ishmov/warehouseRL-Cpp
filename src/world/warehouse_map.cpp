#include "warehouse/world/warehouse_map.hpp"

namespace wrl {
namespace {

constexpr float kWallThickness = 0.3f;

void addPerimeter(WarehouseMap& m) {
    const float t = kWallThickness;
    m.walls = {
        {-t, -t, m.width + 2 * t, t},        // inferior
        {-t, m.height, m.width + 2 * t, t},  // superior
        {-t, 0, t, m.height},                // izquierdo
        {m.width, 0, t, m.height},           // derecho
    };
}

}  // namespace

float WarehouseMap::frictionAt(Vec2 p, float nominal) const {
    float mu = nominal;
    for (const auto& z : frictionZones)
        if (z.rect.contains(p)) mu = std::min(mu, z.mu);
    return mu;
}

bool WarehouseMap::inRestricted(Vec2 p) const {
    for (const auto& r : restricted)
        if (r.contains(p)) return true;
    return false;
}

bool WarehouseMap::insideSolid(Vec2 p, float margin) const {
    if (p.x < margin || p.y < margin || p.x > width - margin || p.y > height - margin) return true;
    for (const auto& s : shelves)
        if (s.distanceTo(p) <= margin) return true;
    return false;
}

WarehouseMap buildMap(const MapSpec& spec, Rng& rng) {
    if (spec.procedural) return generateProcedural(spec.proc, rng);
    WarehouseMap m;
    m.width = spec.width;
    m.height = spec.height;
    m.shelves = spec.shelves;
    m.restricted = spec.restricted;
    m.frictionZones = spec.frictionZones;
    m.pickups = spec.pickups;
    m.dropoffs = spec.dropoffs;
    m.chargers = spec.chargers;
    addPerimeter(m);
    if (spec.robotStart) {
        m.robotStart = *spec.robotStart;
    } else {
        const Vec2 c = m.chargers.front();
        m.robotStart = {{c.x, c.y > m.height * 0.5f ? c.y - 1.0f : c.y + 1.0f},
                        c.y > m.height * 0.5f ? -kPi / 2 : kPi / 2};
    }
    return m;
}

WarehouseMap generateProcedural(const ProceduralParams& p, Rng& rng) {
    WarehouseMap m;
    m.width = p.width;
    m.height = p.height;
    addPerimeter(m);

    // Filas de estanterías dobles (espalda con espalda) separadas por pasillos
    // horizontales y cortadas por pasillos transversales.
    const float rowDepth = 2.0f * p.shelfDepth;
    std::vector<Rect> candidates;
    for (float y = p.margin; y + rowDepth <= p.height - p.margin + 1e-3f; y += rowDepth + p.aisleWidth) {
        for (float x = p.margin; x + p.shelfLength <= p.width - p.margin + 1e-3f; x += p.shelfLength + p.crossAisle)
            candidates.push_back({x, y, p.shelfLength, rowDepth});
    }

    std::vector<Rect> removed;
    for (const auto& r : candidates) {
        if (rng.bernoulli(p.removeShelfProb)) removed.push_back(r);
        else m.shelves.push_back(r);
    }
    // Zonas restringidas en huecos de estanterías retiradas (o en lugar de algunas).
    for (int i = 0; i < p.restrictedZones; ++i) {
        if (!removed.empty()) {
            const int k = rng.uniformInt(0, static_cast<int>(removed.size()) - 1);
            m.restricted.push_back(removed[k].inflated(-0.2f));
            removed.erase(removed.begin() + k);
        } else if (m.shelves.size() > 2) {
            const int k = rng.uniformInt(0, static_cast<int>(m.shelves.size()) - 1);
            m.restricted.push_back(m.shelves[k].inflated(-0.2f));
            m.shelves.erase(m.shelves.begin() + k);
        }
    }

    // Puntos de recogida en las caras de las estanterías, dentro del pasillo.
    const float faceOffset = 0.75f;
    for (const auto& s : m.shelves) {
        for (int side = 0; side < 2; ++side) {
            const float px = s.x + rng.uniform(0.25f, 0.75f) * s.w;
            const float py = side == 0 ? s.y - faceOffset : s.y + s.h + faceOffset;
            const Vec2 pt{px, py};
            if (py < 1.0f || py > p.height - 1.0f) continue;
            bool clear = true;
            for (const auto& o : m.shelves) clear &= o.distanceTo(pt) >= faceOffset - 0.01f;
            for (const auto& o : m.restricted) clear &= o.distanceTo(pt) >= 0.8f;
            if (clear) m.pickups.push_back(pt);
        }
    }

    // Muelles de entrega en la franja inferior y estaciones en la superior.
    const float band = std::min(1.3f, p.margin * 0.45f);
    for (int i = 0; i < p.dropoffs; ++i) {
        const float x = 2.5f + (p.width - 5.0f) * (static_cast<float>(i) + 0.5f) / static_cast<float>(p.dropoffs);
        m.dropoffs.push_back({x, band});
    }
    for (int i = 0; i < p.chargers; ++i) {
        const float x = p.chargers == 1 ? p.width * 0.5f
                                        : 2.0f + (p.width - 4.0f) * static_cast<float>(i) / static_cast<float>(p.chargers - 1);
        m.chargers.push_back({x, p.height - band});
    }

    // Zonas de baja fricción sobre pasillos.
    for (int i = 0; i < p.frictionZones; ++i) {
        const float w = rng.uniform(2.0f, 4.0f), h = rng.uniform(1.5f, 3.0f);
        const float x = rng.uniform(1.0f, p.width - w - 1.0f), y = rng.uniform(1.0f, p.height - h - 1.0f);
        m.frictionZones.push_back({{x, y, w, h}, rng.uniform(p.frictionMin, p.frictionMax)});
    }

    const Vec2 c = m.chargers.front();
    m.robotStart = {{c.x, c.y - 1.0f}, -kPi / 2};
    return m;
}

}  // namespace wrl
