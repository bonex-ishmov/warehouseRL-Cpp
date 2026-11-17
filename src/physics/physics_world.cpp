#include "warehouse/physics/physics_world.hpp"

#include <mutex>
#include <stdexcept>

namespace wrl {
namespace {
// Box2D registra los mundos en un arreglo global (máximo 128) sin sincronizar:
// crear o destruir mundos desde varios hilos a la vez corrompe la memoria.
std::mutex& registryMutex() {
    static std::mutex m;
    return m;
}
}  // namespace

PhysicsWorld::PhysicsWorld() {
    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = {0.0f, 0.0f};
    def.enableSleep = false;
    def.workerCount = 1;  // un hilo por mundo: el paralelismo está en VecEnv
    def.contactHertz = 60.0f;
    {
        std::lock_guard lock(registryMutex());
        world_ = b2CreateWorld(&def);
    }
    if (!b2World_IsValid(world_)) throw std::runtime_error("Box2D no admite más mundos simultáneos (máximo 128)");
    contacts_.reserve(16);
}

PhysicsWorld::~PhysicsWorld() {
    std::lock_guard lock(registryMutex());
    if (b2World_IsValid(world_)) b2DestroyWorld(world_);
}

void PhysicsWorld::buildStatic(const WarehouseMap& map) {
    b2BodyDef bd = b2DefaultBodyDef();
    bd.type = b2_staticBody;
    const b2BodyId ground = b2CreateBody(world_, &bd);

    auto addBox = [&](const Rect& r, BodyTag tag) {
        b2ShapeDef sd = b2DefaultShapeDef();
        sd.material.friction = 0.4f;
        sd.filter.categoryBits = kCatStatic;
        sd.filter.maskBits = kCatRobot | kCatObstacle;
        sd.userData = ShapeTag::encode(tag, -1);
        const b2Polygon box = b2MakeOffsetBox(r.w * 0.5f, r.h * 0.5f, toB2(r.center()), b2Rot_identity);
        b2CreatePolygonShape(ground, &sd, &box);
    };
    for (const auto& w : map.walls) addBox(w, BodyTag::Wall);
    for (const auto& s : map.shelves) addBox(s, BodyTag::Shelf);
}

void PhysicsWorld::step(float dt, int subSteps) {
    b2World_Step(world_, dt, subSteps);
    contacts_.clear();
    const b2ContactEvents ev = b2World_GetContactEvents(world_);
    for (int i = 0; i < ev.beginCount; ++i) {
        const auto& e = ev.beginEvents[i];
        if (!b2Shape_IsValid(e.shapeIdA) || !b2Shape_IsValid(e.shapeIdB)) continue;
        const ShapeTag a = ShapeTag::decode(b2Shape_GetUserData(e.shapeIdA));
        const ShapeTag b = ShapeTag::decode(b2Shape_GetUserData(e.shapeIdB));
        const Vec2 n = fromB2(e.manifold.normal);  // de A hacia B
        if (a.tag == BodyTag::Robot) contacts_.push_back({a, b, n});
        else if (b.tag == BodyTag::Robot) contacts_.push_back({b, a, -n});
    }
}

float PhysicsWorld::castRay(Vec2 origin, Vec2 dir, float range, uint64_t mask) const {
    return castRay(origin, dir, range, mask, nullptr);
}

float PhysicsWorld::castRay(Vec2 origin, Vec2 dir, float range, uint64_t mask, ShapeTag* hit) const {
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = kCatRobot;
    filter.maskBits = mask;
    const b2RayResult r = b2World_CastRayClosest(world_, toB2(origin), toB2(dir * range), filter);
    if (!r.hit) return range;
    if (hit) *hit = ShapeTag::decode(b2Shape_GetUserData(r.shapeId));
    return r.fraction * range;
}

}  // namespace wrl
