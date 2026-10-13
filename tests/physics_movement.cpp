#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

using rtype::ecs::attach;
using rtype::ecs::Ecs;
using rtype::ecs::entity_t;
using rtype::ecs::LocalPosition;
using rtype::ecs::Position;
using rtype::ecs::TransformSystem;
using rtype::physics::Acceleration;
using rtype::physics::MaxSpeed;
using rtype::physics::MovementSystem;
using rtype::physics::Velocity;

TEST_CASE("Movement uses velocity in game units per second", "[physics][movement]")
{
    Ecs ecs;
    ecs.registerSystem<MovementSystem>();

    const entity_t moving = ecs.createEntity();
    ecs.addComponent<Position>(moving, 10.0f, 20.0f);
    ecs.addComponent<Velocity>(moving, 100.0f, -20.0f);

    const entity_t still = ecs.createEntity();
    ecs.addComponent<Position>(still, 30.0f, 40.0f);

    ecs.update(0.5f);

    CHECK(ecs.getComponent<Position>(moving).x == 60.0f);
    CHECK(ecs.getComponent<Position>(moving).y == 10.0f);
    CHECK(ecs.getComponent<Position>(still).x == 30.0f);
    CHECK(ecs.getComponent<Position>(still).y == 40.0f);
}

TEST_CASE("Movement integrates acceleration before position and limits speed magnitude",
          "[physics][movement]")
{
    Ecs ecs;
    ecs.registerSystem<MovementSystem>();

    const entity_t accelerating = ecs.createEntity();
    ecs.addComponent<Position>(accelerating);
    ecs.addComponent<Velocity>(accelerating, 2.0f, 0.0f);
    ecs.addComponent<Acceleration>(accelerating, 4.0f, 2.0f);

    const entity_t limited = ecs.createEntity();
    ecs.addComponent<Position>(limited);
    ecs.addComponent<Velocity>(limited);
    ecs.addComponent<Acceleration>(limited, 12.0f, 16.0f);
    ecs.addComponent<MaxSpeed>(limited, 5.0f);

    const entity_t stopped = ecs.createEntity();
    ecs.addComponent<Position>(stopped);
    ecs.addComponent<Velocity>(stopped, 10.0f, 10.0f);
    ecs.addComponent<MaxSpeed>(stopped, 0.0f);

    ecs.update(0.5f);

    CHECK(ecs.getComponent<Velocity>(accelerating).x == 4.0f);
    CHECK(ecs.getComponent<Velocity>(accelerating).y == 1.0f);
    CHECK(ecs.getComponent<Position>(accelerating).x == 2.0f);
    CHECK(ecs.getComponent<Position>(accelerating).y == 0.5f);

    CHECK(ecs.getComponent<Velocity>(limited).x == 3.0f);
    CHECK(ecs.getComponent<Velocity>(limited).y == 4.0f);
    CHECK(ecs.getComponent<Position>(limited).x == 1.5f);
    CHECK(ecs.getComponent<Position>(limited).y == 2.0f);

    CHECK(ecs.getComponent<Velocity>(stopped).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(stopped).y == 0.0f);
    CHECK(ecs.getComponent<Position>(stopped).x == 0.0f);
    CHECK(ecs.getComponent<Position>(stopped).y == 0.0f);
}

TEST_CASE("Movement changes a child's local position before hierarchy transform", "[physics][movement]")
{
    Ecs ecs;
    ecs.registerSystem<MovementSystem>();
    ecs.registerSystem<TransformSystem>(ecs);

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent, 100.0f, 20.0f);
    ecs.addComponent<Velocity>(parent, 10.0f, 0.0f);

    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent, {5.0f, 2.0f});
    ecs.addComponent<Velocity>(child, 0.0f, 4.0f);

    ecs.update(0.5f);

    CHECK(ecs.getComponent<Position>(parent).x == 105.0f);
    CHECK(ecs.getComponent<LocalPosition>(child).x == 5.0f);
    CHECK(ecs.getComponent<LocalPosition>(child).y == 4.0f);
    CHECK(ecs.getComponent<Position>(child).x == 110.0f);
    CHECK(ecs.getComponent<Position>(child).y == 24.0f);
}

TEST_CASE("Movement can exclude entities without depending on scenes", "[physics][movement]")
{
    Ecs ecs;
    bool allowSecond = false;
    const entity_t first = ecs.createEntity();
    const entity_t second = ecs.createEntity();
    ecs.registerSystem<MovementSystem>([&](entity_t entity) { return entity != second || allowSecond; });

    for (const entity_t entity : {first, second}) {
        ecs.addComponent<Position>(entity);
        ecs.addComponent<Velocity>(entity, 10.0f, 0.0f);
    }

    ecs.update(1.0f);
    CHECK(ecs.getComponent<Position>(first).x == 10.0f);
    CHECK(ecs.getComponent<Position>(second).x == 0.0f);

    allowSecond = true;
    ecs.update(1.0f);
    CHECK(ecs.getComponent<Position>(first).x == 20.0f);
    CHECK(ecs.getComponent<Position>(second).x == 10.0f);
}
