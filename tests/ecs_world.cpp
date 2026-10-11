// Tests of the ECS features used by the game and the interface: entity references, deferred actions, elapsed
// time and hierarchy.

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace rtype::ecs;

namespace
{

// Records the time given to each update
class Clock : public System
{
public:
    std::vector<float> times;

    explicit Clock(ComponentCaches & /*caches*/) {}
    void update(float elapsed) override { times.push_back(elapsed); }
};

} // namespace

TEST_CASE("Systems receive the elapsed time", "[ecs][world]")
{
    Ecs ecs;
    Clock &clock = ecs.registerSystem<Clock>();

    ecs.update(0.016f);
    ecs.update(0.033f);

    REQUIRE(clock.times == std::vector<float>{0.016f, 0.033f});
}

TEST_CASE("A reference to a destroyed entity stays dead when its id is reused", "[ecs][world]")
{
    Ecs ecs;
    const entity_t first = ecs.createEntity();
    const EntityRef reference = ecs.ref(first);
    REQUIRE(ecs.isAlive(reference));

    ecs.destroyEntity(first);
    const entity_t second = ecs.createEntity();

    REQUIRE(second == first);
    REQUIRE_FALSE(ecs.isAlive(reference));
    REQUIRE(ecs.isAlive(ecs.ref(second)));
}

TEST_CASE("Deferred actions run after every system", "[ecs][world]")
{
    Ecs ecs;
    std::vector<int> order;
    ecs.defer([&] { order.push_back(2); });
    ecs.defer([&] { ecs.defer([&] { order.push_back(3); }); });
    order.push_back(1);

    ecs.update(0.0f);

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("A child follows its parent", "[ecs][world]")
{
    Ecs ecs;
    ecs.registerSystem<TransformSystem>(ecs);
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship, 100.0f, 200.0f);
    const entity_t force = ecs.createEntity();
    attach(ecs, force, ship, {40.0f, 0.0f});
    const entity_t glow = ecs.createEntity();
    attach(ecs, glow, force, {0.0f, -5.0f});

    ecs.update(0.0f);
    REQUIRE(ecs.getComponent<Position>(glow).x == 140.0f);
    REQUIRE(ecs.getComponent<Position>(glow).y == 195.0f);

    ecs.getComponent<Position>(ship).x = 300.0f;
    ecs.update(0.0f);
    REQUIRE(ecs.getComponent<Position>(force).x == 340.0f);
    REQUIRE(ecs.getComponent<Position>(glow).x == 340.0f);
}

TEST_CASE("Children are computed after their parent whatever their ids", "[ecs][world]")
{
    Ecs ecs;
    ecs.registerSystem<TransformSystem>(ecs);
    // The child is created first: it has the lowest id
    const entity_t child = ecs.createEntity();
    const entity_t parent = ecs.createEntity();
    const entity_t root = ecs.createEntity();
    ecs.addComponent<Position>(root, 10.0f, 10.0f);
    attach(ecs, parent, root, {5.0f, 0.0f});
    attach(ecs, child, parent, {1.0f, 0.0f});

    ecs.update(0.0f);

    REQUIRE(ecs.getComponent<Position>(child).x == 16.0f);
}

TEST_CASE("Destroying an entity destroys its children", "[ecs][world]")
{
    Ecs ecs;
    const entity_t boss = ecs.createEntity();
    const entity_t turret = ecs.createEntity();
    const entity_t barrel = ecs.createEntity();
    const entity_t other = ecs.createEntity();
    attach(ecs, turret, boss);
    attach(ecs, barrel, turret);

    ecs.destroyEntity(boss);

    REQUIRE_FALSE(ecs.isAlive(turret));
    REQUIRE_FALSE(ecs.isAlive(barrel));
    REQUIRE(ecs.isAlive(other));
}

TEST_CASE("A hidden parent hides its children", "[ecs][world]")
{
    Ecs ecs;
    const entity_t dropdown = ecs.createEntity();
    const entity_t option = ecs.createEntity();
    attach(ecs, option, dropdown);
    REQUIRE_FALSE(isHidden(ecs, option));

    ecs.addComponent<Hidden>(dropdown);

    REQUIRE(isHidden(ecs, dropdown));
    REQUIRE(isHidden(ecs, option));
}
