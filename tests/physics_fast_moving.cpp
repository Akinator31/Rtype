#include "rtype/ecs/Ecs.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::ecs::Ecs;
using rtype::ecs::Position;
using rtype::physics::Aabb;
using rtype::physics::Collider;
using rtype::physics::CollisionMask;
using rtype::physics::CollisionSystem;
using rtype::physics::FastMoving;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Static;
using rtype::physics::Velocity;

TEST_CASE("FastMoving detects a thin target crossed between fixed steps", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}}, rtype::physics::Vec3{}, true);

    const auto target = ecs.createEntity();
    ecs.addComponent<Position>(target);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    CHECK(collisions.collisions().empty());
    CHECK(ecs.getComponent<Position>(shot).x == 5.0f);

    ecs.getComponent<Position>(shot).x = -5.0f;
    ecs.addComponent<FastMoving>(shot);
    ecs.update(0.5f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().a == ecs.ref(shot));
    CHECK(collisions.collisions().front().b == ecs.ref(target));
    CHECK(collisions.collisions().front().normal.x == 1.0f);
    CHECK(collisions.collisions().front().depth == 0.0f);
    CHECK(ecs.getComponent<Position>(shot).x == 5.0f);
}

TEST_CASE("FastMoving solid stops at the first static wall", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 2.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    ecs.addComponent<FastMoving>(shot);

    const auto wall = ecs.createEntity();
    ecs.addComponent<Position>(wall);
    ecs.addComponent<Collider>(wall, Aabb{{}, {0.25f, 10.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    ecs.update(0.5f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(ecs.getComponent<Position>(shot).x == -0.5f);
    CHECK(ecs.getComponent<Velocity>(shot).x == 0.0f);
    CHECK(ecs.getComponent<Velocity>(shot).y == 2.0f);
}

TEST_CASE("FastMoving solid uses the nearest static wall", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    ecs.addComponent<FastMoving>(shot);

    for (const float x : {2.0f, 0.0f}) {
        const auto wall = ecs.createEntity();
        ecs.addComponent<Position>(wall, x, 0.0f);
        ecs.addComponent<Collider>(wall, Aabb{{}, {0.25f, 2.0f, 0.0f}});
        ecs.addComponent<Static>(wall);
    }

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(shot).x == -0.5f);
    CHECK(ecs.getComponent<Velocity>(shot).x == 0.0f);
}

TEST_CASE("FastMoving stops when the wall has the lower entity id", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto wall = ecs.createEntity();
    ecs.addComponent<Position>(wall);
    ecs.addComponent<Collider>(wall, Aabb{{}, {0.25f, 2.0f, 0.0f}});
    ecs.addComponent<Static>(wall);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    ecs.addComponent<FastMoving>(shot);

    ecs.update(0.5f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().normal.x == -1.0f);
    CHECK(ecs.getComponent<Position>(shot).x == -0.5f);
    CHECK(ecs.getComponent<Velocity>(shot).x == 0.0f);
}

TEST_CASE("FastMoving still obeys collision masks", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    ecs.addComponent<FastMoving>(shot);
    ecs.addComponent<CollisionMask>(shot, 0u);

    const auto target = ecs.createEntity();
    ecs.addComponent<Position>(target);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    CHECK(collisions.collisions().empty());
    CHECK(ecs.getComponent<Position>(shot).x == 5.0f);
}

TEST_CASE("FastMoving finds a target that also moved during the step", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}}, rtype::physics::Vec3{}, true);
    ecs.addComponent<FastMoving>(shot);

    const auto target = ecs.createEntity();
    ecs.addComponent<Position>(target, 5.0f, 0.0f);
    ecs.addComponent<Velocity>(target, -20.0f, 0.0f);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().normal.x == 1.0f);
}

TEST_CASE("FastMoving does not report a target outside its swept path", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -5.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 20.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    ecs.addComponent<FastMoving>(shot);

    const auto target = ecs.createEntity();
    ecs.addComponent<Position>(target, 0.0f, 1.0f);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    CHECK(collisions.collisions().empty());
}

TEST_CASE("FastMoving detects contact when a step begins at a wall", "[physics][fast-moving]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const auto shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -0.5f, 0.0f);
    ecs.addComponent<Velocity>(shot, 10.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}}, rtype::physics::Vec3{}, true);
    ecs.addComponent<FastMoving>(shot);

    const auto target = ecs.createEntity();
    ecs.addComponent<Position>(target);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().normal.x == 1.0f);
}
