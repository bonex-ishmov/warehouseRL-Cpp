#include "warehouse/planning/astar.hpp"

#include <queue>

namespace wrl {

AStarPlanner::AStarPlanner(const OccupancyGrid* grid, float clearanceCost, float preferredClearance)
    : grid_(nullptr), clearanceCost_(clearanceCost), preferred_(preferredClearance) {
    setGrid(grid);
}

void AStarPlanner::setGrid(const OccupancyGrid* grid) {
    grid_ = grid;
    if (!grid_) return;
    const size_t n = static_cast<size_t>(grid_->width() * grid_->height());
    g_.assign(n, 0.0f);
    parent_.assign(n, -1);
    stamp_.assign(n, 0);
    closed_.assign(n, 0);
    generation_ = 0;
}

std::optional<std::vector<Cell>> AStarPlanner::search(Cell start, Cell goal, float clearanceCost) {
    const OccupancyGrid& G = *grid_;
    ++generation_;
    expanded_ = 0;
    const int W = G.width();
    auto octile = [&](Cell a) {
        const float dx = static_cast<float>(std::abs(a.x - goal.x)), dy = static_cast<float>(std::abs(a.y - goal.y));
        return G.resolution() * (std::max(dx, dy) + 0.41421356f * std::min(dx, dy));
    };
    auto touch = [&](int i) {
        if (stamp_[i] != generation_) {
            stamp_[i] = generation_;
            g_[i] = std::numeric_limits<float>::max();
            parent_[i] = -1;
            closed_[i] = 0;
        }
    };

    using Item = std::pair<float, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
    const int si = G.index(start), gi = G.index(goal);
    touch(si);
    g_[si] = 0.0f;
    open.push({octile(start), si});

    constexpr int dx[] = {1, -1, 0, 0, 1, 1, -1, -1};
    constexpr int dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty()) {
        const auto [f, ci] = open.top();
        open.pop();
        if (closed_[ci]) continue;
        closed_[ci] = 1;
        ++expanded_;
        if (ci == gi) break;
        const Cell c{ci % W, ci / W};
        for (int k = 0; k < 8; ++k) {
            const Cell n{c.x + dx[k], c.y + dy[k]};
            if (G.blocked(n)) continue;
            // Sin cortar esquinas en diagonales.
            if (k >= 4 && (G.blocked({c.x + dx[k], c.y}) || G.blocked({c.x, c.y + dy[k]}))) continue;
            const int ni = G.index(n);
            touch(ni);
            if (closed_[ni]) continue;
            const float step = G.resolution() * (k < 4 ? 1.0f : 1.41421356f);
            const float near = std::max(0.0f, preferred_ - G.clearance(n)) / preferred_;
            const float cost = g_[ci] + step * (1.0f + clearanceCost * near);
            if (cost < g_[ni]) {
                g_[ni] = cost;
                parent_[ni] = ci;
                open.push({cost + octile(n), ni});
            }
        }
    }
    if (stamp_[gi] != generation_ || !closed_[gi]) return std::nullopt;

    std::vector<Cell> cells;
    for (int i = gi; i != -1; i = parent_[i]) cells.push_back({i % W, i / W});
    std::reverse(cells.begin(), cells.end());
    return cells;
}

std::vector<Vec2> AStarPlanner::smooth(const std::vector<Cell>& cells, Vec2 start, Vec2 goal) const {
    std::vector<Vec2> raw;
    raw.reserve(cells.size() + 2);
    raw.push_back(start);
    for (size_t i = 1; i + 1 < cells.size(); ++i) raw.push_back(grid_->toWorld(cells[i]));
    raw.push_back(goal);

    // Tirar de la cuerda: desde cada punto ancla saltar al más lejano visible.
    std::vector<Vec2> out{raw.front()};
    size_t anchor = 0;
    while (anchor + 1 < raw.size()) {
        size_t next = anchor + 1;
        for (size_t j = raw.size() - 1; j > anchor + 1; --j) {
            if (grid_->lineFree(raw[anchor], raw[j])) {
                next = j;
                break;
            }
        }
        out.push_back(raw[next]);
        anchor = next;
    }
    return out;
}

std::optional<Path> AStarPlanner::plan(Vec2 start, Vec2 goal) {
    if (!grid_) return std::nullopt;
    const auto s = grid_->nearestFree(grid_->toCell(start));
    const auto g = grid_->nearestFree(grid_->toCell(goal));
    if (!s || !g) return std::nullopt;
    if (*s == *g) return Path({start, goal});
    const auto cells = search(*s, *g, clearanceCost_);
    if (!cells) return std::nullopt;
    return Path(smooth(*cells, start, goal));
}

std::optional<float> AStarPlanner::shortestLength(Vec2 start, Vec2 goal) {
    if (!grid_) return std::nullopt;
    const auto s = grid_->nearestFree(grid_->toCell(start));
    const auto g = grid_->nearestFree(grid_->toCell(goal));
    if (!s || !g) return std::nullopt;
    if (*s == *g) return distance(start, goal);
    const auto cells = search(*s, *g, 0.0f);
    if (!cells) return std::nullopt;
    return Path(smooth(*cells, start, goal)).length();
}

}  // namespace wrl
