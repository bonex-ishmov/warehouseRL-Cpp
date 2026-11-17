#include "warehouse/world/occupancy_grid.hpp"

#include <deque>
#include <limits>

namespace wrl {

OccupancyGrid::OccupancyGrid(const WarehouseMap& map, float resolution, float inflation)
    : w_(static_cast<int>(std::ceil(map.width / resolution))),
      h_(static_cast<int>(std::ceil(map.height / resolution))),
      res_(resolution),
      inflation_(inflation) {
    constexpr float kInf = std::numeric_limits<float>::max() / 4;
    clearance_.assign(static_cast<size_t>(w_ * h_), kInf);

    // Celdas ocupadas: estanterías y zonas restringidas (los muros están fuera).
    auto mark = [&](const Rect& r) {
        const Cell lo = toCell({r.x, r.y}), hi = toCell({r.x + r.w, r.y + r.h});
        for (int y = std::max(0, lo.y); y <= std::min(h_ - 1, hi.y); ++y)
            for (int x = std::max(0, lo.x); x <= std::min(w_ - 1, hi.x); ++x)
                if (r.inflated(res_ * 0.5f).contains(toWorld({x, y}))) clearance_[index({x, y})] = 0.0f;
    };
    for (const auto& s : map.shelves) mark(s);
    for (const auto& r : map.restricted) mark(r);

    // Distancia a los muros perimetrales como cota inicial.
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            const Vec2 p = toWorld({x, y});
            const float wall = std::min({p.x, p.y, map.width - p.x, map.height - p.y});
            float& c = clearance_[index({x, y})];
            c = std::min(c, std::max(0.0f, wall));
        }

    // Transformada de distancia por chaflán (dos pasadas, 3-4 aproximando sqrt(2)).
    const float d1 = res_, d2 = res_ * 1.41421356f;
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            float& c = clearance_[index({x, y})];
            if (x > 0) c = std::min(c, clearance_[index({x - 1, y})] + d1);
            if (y > 0) {
                c = std::min(c, clearance_[index({x, y - 1})] + d1);
                if (x > 0) c = std::min(c, clearance_[index({x - 1, y - 1})] + d2);
                if (x + 1 < w_) c = std::min(c, clearance_[index({x + 1, y - 1})] + d2);
            }
        }
    for (int y = h_ - 1; y >= 0; --y)
        for (int x = w_ - 1; x >= 0; --x) {
            float& c = clearance_[index({x, y})];
            if (x + 1 < w_) c = std::min(c, clearance_[index({x + 1, y})] + d1);
            if (y + 1 < h_) {
                c = std::min(c, clearance_[index({x, y + 1})] + d1);
                if (x + 1 < w_) c = std::min(c, clearance_[index({x + 1, y + 1})] + d2);
                if (x > 0) c = std::min(c, clearance_[index({x - 1, y + 1})] + d2);
            }
        }
}

Cell OccupancyGrid::toCell(Vec2 p) const {
    return {static_cast<int>(std::floor(p.x / res_)), static_cast<int>(std::floor(p.y / res_))};
}

std::optional<Cell> OccupancyGrid::nearestFree(Cell c, int maxRadius) const {
    if (inBounds(c) && !blocked(c)) return c;
    for (int r = 1; r <= maxRadius; ++r) {
        std::optional<Cell> best;
        float bestClear = -1.0f;
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                const Cell n{c.x + dx, c.y + dy};
                if (inBounds(n) && !blocked(n) && clearance(n) > bestClear) {
                    best = n;
                    bestClear = clearance(n);
                }
            }
        if (best) return best;
    }
    return std::nullopt;
}

bool OccupancyGrid::lineFree(Vec2 a, Vec2 b) const {
    const float len = distance(a, b);
    const int steps = std::max(1, static_cast<int>(std::ceil(len / (res_ * 0.5f))));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (blocked(toCell(a + (b - a) * t))) return false;
    }
    return true;
}

std::vector<uint8_t> OccupancyGrid::reachable(Cell start) const {
    std::vector<uint8_t> seen(static_cast<size_t>(w_ * h_), 0);
    if (!inBounds(start) || blocked(start)) return seen;
    std::deque<Cell> q{start};
    seen[index(start)] = 1;
    while (!q.empty()) {
        const Cell c = q.front();
        q.pop_front();
        constexpr int dx[] = {1, -1, 0, 0}, dy[] = {0, 0, 1, -1};
        for (int k = 0; k < 4; ++k) {
            const Cell n{c.x + dx[k], c.y + dy[k]};
            if (inBounds(n) && !blocked(n) && !seen[index(n)]) {
                seen[index(n)] = 1;
                q.push_back(n);
            }
        }
    }
    return seen;
}

}  // namespace wrl
