#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using rtype::ecs::Ecs;
using rtype::ecs::LocalPosition;
using rtype::ecs::Position;
using rtype::ecs::TransformSystem;
using rtype::ecs::attach;
using rtype::ecs::entity_t;
using rtype::physics::Aabb;
using rtype::physics::BoundsSystem;
using rtype::physics::Collider;
using rtype::physics::ConfinedToBounds;
using rtype::physics::DestroyOutOfBounds;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Sphere;
using rtype::physics::Static;
using rtype::physics::Vec3;
using rtype::physics::Velocity;

TEST_CASE("BoundsSystem confines a collider using its size and offset", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship, 9.0f, -2.0f);
    ecs.addComponent<Velocity>(ship, 4.0f, -3.0f);
    ecs.addComponent<Collider>(ship, Aabb{{}, {2.0f, 1.0f, 0.0f}}, Vec3{1.0f, 0.0f, 0.0f});
    ecs.addComponent<ConfinedToBounds>(ship, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});

    ecs.update(0.0f);
    CHECK(ecs.getComponent<Position>(ship).x == 7.0f);
    CHECK(ecs.getComponent<Position>(ship).y == 1.0f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(ship).y == 0.0f);
}

TEST_CASE("BoundsSystem keeps velocity that points back into the zone", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship, -2.0f, 5.0f);
    ecs.addComponent<Velocity>(ship, 3.0f, 1.0f);
    ecs.addComponent<ConfinedToBounds>(ship, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});

    ecs.update(0.0f);
    CHECK(ecs.getComponent<Position>(ship).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 3.0f);
    CHECK(ecs.getComponent<Velocity>(ship).y == 1.0f);
}

TEST_CASE("BoundsSystem centers a collider too large for its zone", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<Position>(ship, 9.0f, 5.0f);
    ecs.addComponent<Velocity>(ship, 4.0f, 0.0f);
    ecs.addComponent<Collider>(ship, Aabb{{}, {6.0f, 1.0f, 0.0f}});
    ecs.addComponent<ConfinedToBounds>(ship, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});

    ecs.update(0.0f);
    CHECK(ecs.getComponent<Position>(ship).x == 5.0f);
    CHECK(ecs.getComponent<Velocity>(ship).x == 0.0f);
}

TEST_CASE("BoundsSystem destroys only after the whole collider leaves the margin", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, 13.0f, 5.0f);
    ecs.addComponent<Collider>(shot, Sphere{{}, 1.0f});
    ecs.addComponent<DestroyOutOfBounds>(shot, Position{0.0f, 0.0f},
                                          Position{10.0f, 10.0f}, 2.0f);

    ecs.update(0.0f);
    CHECK(ecs.isAlive(shot));

    ecs.getComponent<Position>(shot).x = 13.1f;
    ecs.update(0.0f);
    CHECK_FALSE(ecs.isAlive(shot));
}

TEST_CASE("BoundsSystem uses the collider center for out of bounds destruction", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, 7.0f, 5.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {1.0f, 1.0f, 0.0f}}, Vec3{5.0f, 0.0f, 0.0f});
    ecs.addComponent<DestroyOutOfBounds>(shot, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});

    ecs.update(0.0f);
    CHECK_FALSE(ecs.isAlive(shot));
}

TEST_CASE("BoundsSystem sees the current position of an attached child", "[physics][bounds]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    physics.registerSystem<BoundsSystem>(ecs);
    physics.registerSystem<TransformSystem>(ecs);

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent);
    ecs.addComponent<Velocity>(parent, 2.0f, 0.0f);

    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent, {8.0f, 0.0f});
    ecs.addComponent<ConfinedToBounds>(child, Position{0.0f, 0.0f}, Position{8.0f, 10.0f});

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(child).x == 8.0f);
    CHECK(ecs.getComponent<LocalPosition>(child).x == 7.0f);

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(child).x == 8.0f);
    CHECK(ecs.getComponent<LocalPosition>(child).x == 6.0f);
}

TEST_CASE("BoundsSystem defers destruction across multiple fixed steps", "[physics][bounds]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 2);
    physics.registerSystem<BoundsSystem>(ecs);
    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, 20.0f, 0.0f);
    ecs.addComponent<DestroyOutOfBounds>(shot, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});
    const auto reference = ecs.ref(shot);

    ecs.update(1.0f);
    CHECK_FALSE(ecs.isAlive(reference));
    CHECK(ecs.createEntity() == shot);
    CHECK_FALSE(ecs.isAlive(reference));
}

TEST_CASE("BoundsSystem destroys a child using its current world position", "[physics][bounds]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    physics.registerSystem<BoundsSystem>(ecs);
    physics.registerSystem<TransformSystem>(ecs);

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent);
    ecs.addComponent<Velocity>(parent, 2.0f, 0.0f);
    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent, {11.0f, 0.0f});
    ecs.addComponent<DestroyOutOfBounds>(child, Position{0.0f, 0.0f},
                                          Position{10.0f, 10.0f});
    const auto reference = ecs.ref(child);

    ecs.update(0.5f);
    CHECK(ecs.isAlive(parent));
    CHECK_FALSE(ecs.isAlive(reference));
}

TEST_CASE("BoundsSystem filters inactive entities", "[physics][bounds]")
{
    Ecs ecs;
    bool active = false;
    ecs.registerSystem<BoundsSystem>(ecs, [&](entity_t) { return active; });
    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, 20.0f, 0.0f);
    ecs.addComponent<DestroyOutOfBounds>(shot, Position{0.0f, 0.0f}, Position{10.0f, 10.0f});

    ecs.update(0.0f);
    CHECK(ecs.isAlive(shot));
    active = true;
    ecs.update(0.0f);
    CHECK_FALSE(ecs.isAlive(shot));
}

TEST_CASE("BoundsSystem rejects invalid zones", "[physics][bounds]")
{
    Ecs ecs;
    ecs.registerSystem<BoundsSystem>(ecs);
    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot);
    ecs.addComponent<DestroyOutOfBounds>(shot, Position{10.0f, 0.0f},
                                          Position{0.0f, 10.0f});
    CHECK_THROWS_AS(ecs.update(0.0f), std::invalid_argument);
}
