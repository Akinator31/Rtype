#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

using rtype::ecs::attach;
using rtype::ecs::ComponentCaches;
using rtype::ecs::Ecs;
using rtype::ecs::entity_t;
using rtype::ecs::EntityRef;
using rtype::ecs::Position;
using rtype::ecs::System;
using rtype::ecs::TransformSystem;
using rtype::physics::Aabb;
using rtype::physics::Collider;
using rtype::physics::CollisionLayer;
using rtype::physics::CollisionMask;
using rtype::physics::CollisionSystem;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Sphere;
using rtype::physics::Vec3;
using rtype::physics::Velocity;

namespace
{

class DestroyOnCollision : public System
{
    Ecs &_ecs;
    CollisionSystem &_collisions;

public:
    std::vector<EntityRef> victims;

    DestroyOnCollision(ComponentCaches &, Ecs &ecs, CollisionSystem &collisions)
        : _ecs(ecs), _collisions(collisions)
    {
    }

    void update(float) override
    {
        for (const auto &collision : _collisions.collisions()) {
            victims.push_back(collision.b);
            const EntityRef victim = collision.b;
            _ecs.defer([this, victim] {
                if (_ecs.isAlive(victim)) {
                    _ecs.destroyEntity(victim.id);
                }
            });
        }
    }
};

} // namespace

TEST_CASE("CollisionSystem reports AABB contacts with entity references", "[physics][collision]")
{
    Ecs ecs;
    auto &system = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    const entity_t first = ecs.createEntity();
    ecs.addComponent<Position>(first, 0.0f, 0.0f);
    ecs.addComponent<Collider>(first, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t second = ecs.createEntity();
    ecs.addComponent<Position>(second, 2.0f, 0.0f);
    ecs.addComponent<Collider>(second, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t withoutPosition = ecs.createEntity();
    ecs.addComponent<Collider>(withoutPosition, Aabb{{}, {2.0f, 2.0f, 0.0f}});

    ecs.update(0.0f);
    REQUIRE(system.collisions().size() == 1);
    const auto &collision = system.collisions().front();
    CHECK(collision.a == ecs.ref(first));
    CHECK(collision.b == ecs.ref(second));
    CHECK(collision.normal.x == 1.0f);
    CHECK(collision.depth == 0.0f);

    ecs.getComponent<Position>(second).x = 3.0f;
    ecs.update(0.0f);
    CHECK(system.collisions().empty());
}

TEST_CASE("CollisionSystem applies both layer masks and default filtering", "[physics][collision]")
{
    Ecs ecs;
    auto &system = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    const entity_t player = ecs.createEntity();
    const entity_t enemy = ecs.createEntity();
    for (const entity_t entity : {player, enemy}) {
        ecs.addComponent<Position>(entity);
        ecs.addComponent<Collider>(entity, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    }

    ecs.update(0.0f);
    REQUIRE(system.collisions().size() == 1);

    ecs.addComponent<CollisionLayer>(player, 1u << 0);
    ecs.addComponent<CollisionMask>(player, 1u << 1);
    ecs.addComponent<CollisionLayer>(enemy, 1u << 1);
    ecs.addComponent<CollisionMask>(enemy, 1u << 0);
    ecs.update(0.0f);
    REQUIRE(system.collisions().size() == 1);

    ecs.getComponent<CollisionMask>(enemy).bits = 1u << 1;
    ecs.update(0.0f);
    CHECK(system.collisions().empty());
}

TEST_CASE("CollisionSystem checks exact shapes after broad phase", "[physics][collision]")
{
    Ecs ecs;
    auto &system = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    const entity_t box = ecs.createEntity();
    ecs.addComponent<Position>(box);
    ecs.addComponent<Collider>(box, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const entity_t sphere = ecs.createEntity();
    ecs.addComponent<Position>(sphere, 1.9f, 1.9f);
    ecs.addComponent<Collider>(sphere, Sphere{{}, 1.0f}, Vec3{}, true);

    ecs.update(0.0f);
    CHECK(system.collisions().empty());

    ecs.getComponent<Position>(sphere) = {1.5f, 0.0f};
    ecs.update(0.0f);
    REQUIRE(system.collisions().size() == 1);
    CHECK(system.collisions().front().a == ecs.ref(box));
    CHECK(system.collisions().front().b == ecs.ref(sphere));
    CHECK(system.collisions().front().normal.x == 1.0f);
    CHECK(system.collisions().front().depth == 0.5f);
    CHECK(ecs.getComponent<Position>(sphere).x == 1.5f);
}

TEST_CASE("CollisionSystem includes collider offsets and sphere pairs", "[physics][collision]")
{
    Ecs ecs;
    auto &system = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    const entity_t first = ecs.createEntity();
    ecs.addComponent<Position>(first, 0.0f, 0.0f);
    ecs.addComponent<Collider>(first, Sphere{{}, 1.0f}, Vec3{5.0f, 0.0f, 0.0f});

    const entity_t second = ecs.createEntity();
    ecs.addComponent<Position>(second, 7.0f, 0.0f);
    ecs.addComponent<Collider>(second, Sphere{{}, 1.0f});

    ecs.update(0.0f);
    REQUIRE(system.collisions().size() == 1);
    CHECK(system.collisions().front().depth == 0.0f);
}

TEST_CASE("CollisionSystem keeps a first-step hit for the whole frame", "[physics][collision]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 2);
    physics.registerSystem<MovementSystem>();
    auto &system = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t shot = ecs.createEntity();
    ecs.addComponent<Position>(shot, -2.0f, 0.0f);
    ecs.addComponent<Velocity>(shot, 4.0f, 0.0f);
    ecs.addComponent<Collider>(shot, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    const entity_t target = ecs.createEntity();
    ecs.addComponent<Position>(target);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(1.0f);
    REQUIRE(system.collisions().size() == 1);
    CHECK(system.collisions().front().a == ecs.ref(shot));
    CHECK(ecs.getComponent<Position>(shot).x == 2.0f);

    ecs.update(0.0f);
    CHECK(system.collisions().empty());
}

TEST_CASE("CollisionSystem emits a persistent pair once per frame", "[physics][collision]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 2);
    auto &system = physics.registerSystem<CollisionSystem>(ecs, 1.0f);
    for (int i = 0; i < 2; ++i) {
        const entity_t entity = ecs.createEntity();
        ecs.addComponent<Position>(entity);
        ecs.addComponent<Collider>(entity, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    }

    ecs.update(1.0f);
    CHECK(system.collisions().size() == 1);
    ecs.update(0.5f);
    CHECK(system.collisions().size() == 1);
}

TEST_CASE("Game systems can consume collisions and defer destruction", "[physics][collision]")
{
    Ecs ecs;
    auto &system = ecs.registerSystem<CollisionSystem>(ecs, 1.0f);
    auto &damage = ecs.registerSystem<DestroyOnCollision>(ecs, system);
    const entity_t first = ecs.createEntity();
    const entity_t second = ecs.createEntity();
    for (const entity_t entity : {first, second}) {
        ecs.addComponent<Position>(entity);
        ecs.addComponent<Collider>(entity, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    }

    const EntityRef victim = ecs.ref(second);
    ecs.update(0.0f);
    REQUIRE(damage.victims.size() == 1);
    CHECK(damage.victims.front() == victim);
    CHECK_FALSE(ecs.isAlive(victim));

    const entity_t reused = ecs.createEntity();
    CHECK(reused == second);
    CHECK_FALSE(ecs.isAlive(victim));
}

TEST_CASE("CollisionSystem uses current world position of attached entities", "[physics][collision]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.5f, 1);
    physics.registerSystem<MovementSystem>();
    physics.registerSystem<TransformSystem>(ecs);
    auto &system = physics.registerSystem<CollisionSystem>(ecs, 1.0f);

    const entity_t parent = ecs.createEntity();
    ecs.addComponent<Position>(parent);
    ecs.addComponent<Velocity>(parent, 2.0f, 0.0f);
    const entity_t child = ecs.createEntity();
    attach(ecs, child, parent, {1.0f, 0.0f});
    ecs.addComponent<Collider>(child, Aabb{{}, {0.25f, 0.25f, 0.0f}});
    const entity_t target = ecs.createEntity();
    ecs.addComponent<Position>(target, 2.0f, 0.0f);
    ecs.addComponent<Collider>(target, Aabb{{}, {0.25f, 0.25f, 0.0f}});

    ecs.update(0.5f);
    CHECK(ecs.getComponent<Position>(child).x == 2.0f);
    REQUIRE(system.collisions().size() == 1);
    CHECK(system.collisions().front().a == ecs.ref(child));
    CHECK(system.collisions().front().b == ecs.ref(target));
}

TEST_CASE("CollisionSystem validates its grid configuration", "[physics][collision]")
{
    Ecs ecs;
    CHECK_THROWS_AS(ecs.registerSystem<CollisionSystem>(ecs, 0.0f), std::invalid_argument);
}
