#pragma once

#include <cstdint>
#include <vector>

#include <box2d/box2d.h>

#include "warehouse/core/math.hpp"
#include "warehouse/world/warehouse_map.hpp"

namespace wrl {

enum CollisionCategory : uint64_t {
    kCatStatic = 1u << 0,
    kCatRobot = 1u << 1,
    kCatObstacle = 1u << 2,
};

enum class BodyTag : uint8_t { Wall, Shelf, Robot, Forklift, Worker };

/// Etiqueta que se guarda en el userData de cada forma de Box2D.
struct ShapeTag {
    BodyTag tag;
    int index;  ///< índice del obstáculo dinámico o del robot; -1 para estáticos

    static void* encode(BodyTag t, int i) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>((static_cast<uint32_t>(i + 1) << 8) | static_cast<uint8_t>(t)));
    }
    static ShapeTag decode(void* p) {
        const auto v = reinterpret_cast<uintptr_t>(p);
        return {static_cast<BodyTag>(v & 0xFF), static_cast<int>(v >> 8) - 1};
    }
};

struct ContactBegin {
    ShapeTag self;   ///< forma del robot involucrada
    ShapeTag other;  ///< lo que tocó
    Vec2 normal;     ///< normal del contacto
};

/// Envoltura RAII de un mundo Box2D sin gravedad (vista cenital).
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void buildStatic(const WarehouseMap& map);
    void step(float dt, int subSteps);

    b2WorldId id() const { return world_; }
    /// Contactos que empezaron en el último paso y que involucran al robot.
    const std::vector<ContactBegin>& robotContacts() const { return contacts_; }

    /// Distancia al primer impacto a lo largo de dir (unitario), o range si no hay.
    float castRay(Vec2 origin, Vec2 dir, float range, uint64_t mask) const;
    /// Igual que castRay pero también informa qué tocó.
    float castRay(Vec2 origin, Vec2 dir, float range, uint64_t mask, ShapeTag* hit) const;

private:
    b2WorldId world_{};
    std::vector<ContactBegin> contacts_;
};

inline b2Vec2 toB2(Vec2 v) { return {v.x, v.y}; }
inline Vec2 fromB2(b2Vec2 v) { return {v.x, v.y}; }

}  // namespace wrl
