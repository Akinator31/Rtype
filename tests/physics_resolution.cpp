#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::ecs::attach;
using rtype::ecs::Ecs;
using rtype::ecs::entity_t;
using rtype::ecs::LocalPosition;
using rtype::ecs::Position;
using rtype::ecs::TransformSystem;
using rtype::physics::Aabb;
using rtype::physics::Acceleration;
using rtype::physics::Collider;
using rtype::physics::CollisionSystem;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Sphere;
using rtype::physics::Static;
using rtype::physics::Velocity;

TEST_CASE("A mobile collider slides along a static wall", "[physics][resolution]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship);
    ecs.addComponent<Velocity>(ship, 2.0f, 1.0f);
    ecs.addComponent<Collider>(ship, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t wall = ecs.createEntity();
    ecs.addComponent<Position>(wall, 2.0f, 0.0f);
    ecs.addComponent<Velocity>(wall, 100.0f, 0.0f);
    ecs.addComponent<Collider>(wall, Aabb{{}, {1.0f, 10.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    ecs.update(0.5f);

    CHECK(ecs.getComponent<Position>(ship).x == 0.0f);
    CHECK(ecs.getComponent<Position>(ship).y == 0.5f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(ship).y == 1.0f);
    CHECK(ecs.getComponent<Position>(wall).x == 2.0f);
    CHECK(ecs.getComponent<Velocity>(wall).x == 100.0f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().depth == 1.0f);
}

TEST_CASE("A trigger reports contact without blocking movement", "[physics][resolution]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship);
    ecs.addComponent<Velocity>(ship, 2.0f, 0.0f);
    ecs.addComponent<Collider>(ship, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t bonus = ecs.createEntity();
    ecs.addComponent<Position>(bonus, 2.0f, 0.0f);
    ecs.addComponent<Collider>(bonus, Sphere{{}, 1.0f}, rtype::physics::Vec3{}, true);
    ecs.addComponent<Static>(bonus);

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(ship).x == 1.0f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 2.0f);
    CHECK(collisions.collisions().size() == 1);
}

TEST_CASE("Two mobile solids report contact without resolution", "[physics][resolution]")
{
    Ecs ecs;
    auto &collisions = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    const entity_t first = ecs.createEntity();
    ecs.addComponent<Position>(first);
    ecs.addComponent<Velocity>(first, 1.0f, 0.0f);
    ecs.addComponent<Collider>(first, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t second = ecs.createEntity();
    ecs.addComponent<Position>(second, 1.0f, 0.0f);
    ecs.addComponent<Velocity>(second, -1.0f, 0.0f);
    ecs.addComponent<Collider>(second, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    ecs.update(0.0f);
    CHECK(collisions.collisions().size() == 1);
    CHECK(ecs.getComponent<Position>(first).x == 0.0f);
    CHECK(ecs.getComponent<Position>(second).x == 1.0f);
    CHECK(ecs.getComponent<Velocity>(first).x == 1.0f);
    CHECK(ecs.getComponent<Velocity>(second).x == -1.0f);
}

TEST_CASE("A static collider pushes an attached child in local coordinates", "[physics][resolution]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<TransformSystem>(ecs);
    physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent);
    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent);
    ecs.addComponent<Collider>(child, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t wall = ecs.createEntity();
    ecs.addComponent<Position>(wall, 1.5f, 0.0f);
    ecs.addComponent<Collider>(wall, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(child).x == -0.5f);
    CHECK(ecs.getComponent<LocalPosition>(child).x == -0.5f);

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(child).x == -0.5f);
    CHECK(ecs.getComponent<LocalPosition>(child).x == -0.5f);
}

TEST_CASE("Solid resolution repeats on each fixed step even when the event is deduplicated",
          "[physics][resolution]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 2);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship);
    ecs.addComponent<Velocity>(ship);
    ecs.addComponent<Acceleration>(ship, 4.0f, 0.0f);
    ecs.addComponent<Collider>(ship, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t wall = ecs.createEntity();
    ecs.addComponent<Position>(wall, 2.0f, 0.0f);
    ecs.addComponent<Collider>(wall, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    ecs.update(1.0f);
    CHECK(ecs.getComponent<Position>(ship).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 0.0f);
    CHECK(collisions.collisions().size() == 1);
}

TEST_CASE("A mobile sphere is repelled from a static sphere", "[physics][resolution]")
{
    Ecs ecs;
    auto &collisions = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t obstacle = ecs.createEntity();
    ecs.addComponent<Position>(obstacle);
    ecs.addComponent<Collider>(obstacle, Sphere{{}, 1.0f});
    ecs.addComponent<Static>(obstacle);

    const entity_t mobile = ecs.createEntity();
    ecs.addComponent<Position>(mobile, 1.5f, 0.0f);
    ecs.addComponent<Velocity>(mobile, -2.0f, 0.0f);
    ecs.addComponent<Collider>(mobile, Sphere{{}, 1.0f});

    ecs.update(0.0f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(ecs.getComponent<Position>(mobile).x == 2.0f);
    CHECK(ecs.getComponent<Velocity>(mobile).x == 0.0f);
    CHECK(ecs.getComponent<Position>(obstacle).x == 0.0f);
}

TEST_CASE("Resolution keeps velocity already pointing away from a wall", "[physics][resolution]")
{
    Ecs ecs;
    ecs.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t mobile = ecs.createEntity();
    ecs.addComponent<Position>(mobile, 1.0f, 0.0f);
    ecs.addComponent<Velocity>(mobile, -2.0f, 0.0f);
    ecs.addComponent<Collider>(mobile, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t wall = ecs.createEntity();
    ecs.addComponent<Position>(wall, 2.0f, 0.0f);
    ecs.addComponent<Collider>(wall, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    ecs.update(0.0f);
    CHECK(ecs.getComponent<Position>(mobile).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(mobile).x == -2.0f);
}
