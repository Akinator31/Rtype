#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::ecs::attach;
using rtype::ecs::ComponentCaches;
using rtype::ecs::Ecs;
using rtype::ecs::entity_t;
using rtype::ecs::Position;
using rtype::ecs::System;
using rtype::ecs::TransformSystem;
using rtype::physics::Aabb;
using rtype::physics::BoundsSystem;
using rtype::physics::Collider;
using rtype::physics::CollisionSystem;
using rtype::physics::ConfinedToBounds;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Velocity;

namespace
{

class WriteVelocity : public System
{
    Ecs &_ecs;
    entity_t _parent;
    entity_t _child;

public:
    WriteVelocity(ComponentCaches &, Ecs &ecs, entity_t parent, entity_t child)
        : _ecs(ecs), _parent(parent), _child(child)
    {
    }

    void update(float) override
    {
        _ecs.getComponent<Velocity>(_parent) = {4.0f, 0.0f};
        _ecs.getComponent<Velocity>(_child) = {2.0f, 0.0f};
    }
};

class ReadCollision : public System
{
    Ecs &_ecs;
    CollisionSystem &_collisions;
    entity_t _child;

public:
    std::size_t contacts = 0;
    float childX = 0.0f;

    ReadCollision(ComponentCaches &, Ecs &ecs, CollisionSystem &collisions, entity_t child)
        : _ecs(ecs), _collisions(collisions), _child(child)
    {
    }

    void update(float) override
    {
        contacts = _collisions.collisions().size();
        childX = _ecs.getComponent<Position>(_child).x;
    }
};

} // namespace

TEST_CASE("Fixed physics order gives collision readers the child's current world position",
          "[physics][order]")
{
    Ecs ecs;

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent);
    ecs.addComponent<Velocity>(parent);
    ecs.addComponent<ConfinedToBounds>(parent, Position{-10.0f, -10.0f}, Position{1.0f, 10.0f});

    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent, {1.0f, 0.0f});
    ecs.addComponent<Velocity>(child);
    ecs.addComponent<Collider>(child, Aabb{{}, {0.25f, 0.25f, 0.0f}}, rtype::physics::Vec3{}, true);

    const entity_t target = ecs.createEntity();
    ecs.addComponent<Position>(target, 3.0f, 0.0f);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.registerSystem<WriteVelocity>(ecs, parent, child);
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    physics.registerSystem<BoundsSystem>(ecs);
    physics.registerSystem<TransformSystem>(ecs);
    auto &collisions = physics.registerSystem<CollisionSystem>(ecs, 1.0f);
    auto &reader = ecs.registerSystem<ReadCollision>(ecs, collisions, child);

    ecs.update(0.5f);

    CHECK(ecs.getComponent<Position>(parent).x == 1.0f);
    CHECK(ecs.getComponent<Velocity>(parent).x == 0.0f);
    CHECK(ecs.getComponent<rtype::ecs::LocalPosition>(child).x == 2.0f);
    CHECK(ecs.getComponent<Position>(child).x == 3.0f);
    CHECK(reader.childX == 3.0f);
    CHECK(reader.contacts == 1);
    REQUIRE(collisions.collisions().size() == 1);
    CHECK(collisions.collisions().front().a == ecs.ref(child));
    CHECK(collisions.collisions().front().b == ecs.ref(target));
}
