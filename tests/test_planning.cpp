#include <gtest/gtest.h>

#include "warehouse/planning/astar.hpp"
#include "warehouse/planning/path.hpp"
#include "warehouse/world/occupancy_grid.hpp"

using namespace wrl;

namespace {

WarehouseMap mapWithWall() {
    WarehouseMap m;
    m.width = 20;
    m.height = 12;
    m.shelves.push_back({9, 0, 2, 9});  // muro con un paso arriba
    return m;
}

}  // namespace

TEST(Planificacion, AStarRodeaElObstaculo) {
    const auto m = mapWithWall();
    OccupancyGrid grid(m, 0.2f, 0.55f);
    AStarPlanner planner(&grid);
    const auto path = planner.plan({3, 3}, {17, 3});
    ASSERT_TRUE(path.has_value());
    EXPECT_GT(path->length(), 14.0f);  // la recta mide 14 m y está bloqueada
    // Todos los tramos suavizados son libres en el mapa inflado.
    const auto& pts = path->points();
    for (size_t i = 1; i + 2 < pts.size(); ++i) EXPECT_TRUE(grid.lineFree(pts[i], pts[i + 1]));
    // Pasa por encima del muro.
    bool above = false;
    for (const auto& p : pts) above |= p.y > 9.0f;
    EXPECT_TRUE(above);
}

TEST(Planificacion, SinCaminoDevuelveVacio) {
    WarehouseMap m;
    m.width = 20;
    m.height = 12;
    m.shelves.push_back({9, 0, 2, 12});  // muro completo
    OccupancyGrid grid(m, 0.2f, 0.55f);
    AStarPlanner planner(&grid);
    EXPECT_FALSE(planner.plan({3, 3}, {17, 3}).has_value());
}

TEST(Planificacion, ElSuavizadoReduceLosVertices) {
    WarehouseMap m;
    m.width = 20;
    m.height = 20;
    OccupancyGrid grid(m, 0.2f, 0.55f);
    AStarPlanner planner(&grid);
    const auto path = planner.plan({2, 2}, {17, 13});
    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->points().size(), 2u);  // en campo abierto queda la recta
    EXPECT_NEAR(path->length(), distance({2, 2}, {17, 13}), 1e-3f);
}

TEST(Planificacion, LaInflacionRespetaLaHolgura) {
    const auto m = mapWithWall();
    OccupancyGrid grid(m, 0.2f, 0.55f);
    EXPECT_TRUE(grid.blocked(grid.toCell({8.7f, 4.0f})));   // 0.3 m del muro
    EXPECT_FALSE(grid.blocked(grid.toCell({8.0f, 4.0f})));  // 1.0 m del muro
    EXPECT_NEAR(grid.clearanceAt({7.0f, 4.0f}), 2.0f, 0.15f);
}

TEST(Planificacion, ProyeccionSobreLaRuta) {
    Path p({{0, 0}, {4, 0}, {4, 3}});
    EXPECT_FLOAT_EQ(p.length(), 7.0f);
    const auto pr = p.project({2, 1});
    EXPECT_NEAR(pr.s, 2.0f, 1e-5f);
    EXPECT_NEAR(pr.lateral, 1.0f, 1e-5f);
    EXPECT_NEAR(p.remaining({4, 1}), 2.0f, 1e-5f);
    const Vec2 q = p.pointAt(5.5f);
    EXPECT_NEAR(q.x, 4.0f, 1e-5f);
    EXPECT_NEAR(q.y, 1.5f, 1e-5f);
}
