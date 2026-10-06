#include "ecs/Ecs.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ecs;

namespace {

struct Position {
    int x = 0;
};
struct Velocity {
    int dx = 0;
};

class Movement : public System {
    ComponentCache<Position>& _positions;
    ComponentCache<Velocity>& _velocities;

  public:
    explicit Movement(ComponentCaches& caches)
        : _positions(use<Position>(caches)), _velocities(use<Velocity>(caches)) {}

    void update() override {
        for (entity_t e = 0; e < _positions.size(); ++e) {
            if (_positions.has(e) && _velocities.has(e)) {
                _positions.get(e).x += _velocities.get(e).dx;
            }
        }
    }
};

class DestroyAll : public System {
    ComponentCache<Position>& _positions;
    Ecs& _ecs;

  public:
    DestroyAll(ComponentCaches& caches, Ecs& ecs)
        : _positions(use<Position>(caches)), _ecs(ecs) {}

    void update() override {
        for (entity_t e = 0; e < _positions.size(); ++e) {
            if (_positions.has(e)) {
                _ecs.destroyEntity(e);
            }
        }
    }
};

} // namespace

TEST_CASE("A cache stores components by entity id", "[ecs][cache]") {
    ComponentCache<Position> cache;
    REQUIRE_FALSE(cache.has(5));
    cache.emplace(5, 42);
    REQUIRE(cache.has(5));
    REQUIRE_FALSE(cache.has(2));
    REQUIRE(cache.get(5).x == 42);
    cache.erase(5);
    REQUIRE_FALSE(cache.has(5));
    cache.erase(1000); // out of range: no-op
}

TEST_CASE("ComponentCaches hands out one stable cache per type", "[ecs][cache]") {
    ComponentCaches caches;
    auto& positions = caches.cache<Position>();
    caches.cache<Velocity>();
    REQUIRE(&caches.cache<Position>() == &positions);
    positions.emplace(0);
    caches.erase(0);
    REQUIRE_FALSE(positions.has(0));
}

TEST_CASE("Entity ids are recycled", "[ecs]") {
    Ecs ecs;
    const entity_t a = ecs.createEntity();
    const entity_t b = ecs.createEntity();
    REQUIRE(a != b);
    ecs.addComponent<Position>(a, 1);
    ecs.destroyEntity(a);
    REQUIRE_FALSE(ecs.isAlive(a));
    REQUIRE_FALSE(ecs.hasComponent<Position>(a));
    REQUIRE(ecs.createEntity() == a);
    REQUIRE_FALSE(ecs.hasComponent<Position>(a));
}

TEST_CASE("A system only touches entities with all its components", "[ecs][system]") {
    Ecs ecs;
    ecs.registerSystem<Movement>();
    const entity_t moving = ecs.createEntity();
    const entity_t still = ecs.createEntity();
    ecs.addComponent<Position>(moving, 10);
    ecs.addComponent<Velocity>(moving, 3);
    ecs.addComponent<Position>(still, 10);
    ecs.update();
    REQUIRE(ecs.getComponent<Position>(moving).x == 13);
    REQUIRE(ecs.getComponent<Position>(still).x == 10);
}

TEST_CASE("A disabled system is skipped", "[ecs][system]") {
    Ecs ecs;
    auto& movement = ecs.registerSystem<Movement>();
    const entity_t e = ecs.createEntity();
    ecs.addComponent<Position>(e, 0);
    ecs.addComponent<Velocity>(e, 1);
    movement.enabled = false;
    ecs.update();
    REQUIRE(ecs.getComponent<Position>(e).x == 0);
    movement.enabled = true;
    ecs.update();
    REQUIRE(ecs.getComponent<Position>(e).x == 1);
}

TEST_CASE("A system can destroy entities while iterating", "[ecs][system]") {
    Ecs ecs;
    ecs.registerSystem<DestroyAll>(ecs);
    const entity_t a = ecs.createEntity();
    const entity_t b = ecs.createEntity();
    ecs.addComponent<Position>(a);
    ecs.addComponent<Position>(b);
    ecs.update();
    REQUIRE_FALSE(ecs.isAlive(a));
    REQUIRE_FALSE(ecs.isAlive(b));
}

TEST_CASE("A system lists the component types it uses", "[ecs][system]") {
    Ecs ecs;
    const auto& types = ecs.registerSystem<Movement>().componentTypes();
    REQUIRE(types.size() == 2);
    REQUIRE(types[0] == std::type_index(typeid(Position)));
    REQUIRE(types[1] == std::type_index(typeid(Velocity)));
}
