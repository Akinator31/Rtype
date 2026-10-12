#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <vector>

using rtype::ecs::ComponentCaches;
using rtype::ecs::Ecs;
using rtype::ecs::Position;
using rtype::ecs::System;
using rtype::ecs::entity_t;
using rtype::physics::MovementSystem;
using rtype::physics::PhysicsStep;
using rtype::physics::Velocity;

namespace
{

class Recorder : public System
{
    std::vector<float> &_ticks;
    std::vector<int> &_order;
    int _id;

public:
    Recorder(ComponentCaches &, std::vector<float> &ticks, std::vector<int> &order, int id)
        : _ticks(ticks), _order(order), _id(id)
    {
    }

    void update(float elapsed) override
    {
        _ticks.push_back(elapsed);
        _order.push_back(_id);
    }
};

float moveFor(const std::vector<float> &frames, float step = 0.125f, float speed = 8.0f)
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(step, 8);
    physics.registerSystem<MovementSystem>();
    const entity_t entity = ecs.createEntity();
    ecs.addComponent<Position>(entity);
    ecs.addComponent<Velocity>(entity, speed, 0.0f);
    for (const float elapsed : frames) {
        ecs.update(elapsed);
    }
    return ecs.getComponent<Position>(entity).x;
}

}

TEST_CASE("PhysicsStep accumulates partial frames and runs systems in order", "[physics][step]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.25f, 4);
    std::vector<float> firstTicks;
    std::vector<float> secondTicks;
    std::vector<int> order;
    physics.registerSystem<Recorder>(firstTicks, order, 1);
    auto &second = physics.registerSystem<Recorder>(secondTicks, order, 2);

    CHECK(physics.step() == 0.25f);
    CHECK(physics.maxSteps() == 4);
    CHECK(physics.alpha() == 0.0f);

    ecs.update(0.125f);
    CHECK(order.empty());
    CHECK(physics.alpha() == 0.5f);

    ecs.update(0.625f);
    CHECK(order == std::vector<int>{1, 2, 1, 2, 1, 2});
    CHECK(firstTicks == std::vector<float>{0.25f, 0.25f, 0.25f});
    CHECK(secondTicks == firstTicks);
    CHECK(physics.alpha() == 0.0f);

    second.enabled = false;
    ecs.update(0.25f);
    CHECK(order == std::vector<int>{1, 2, 1, 2, 1, 2, 1});
}

TEST_CASE("PhysicsStep gives the same movement for equivalent frame durations", "[physics][step]")
{
    CHECK(moveFor({0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f}) == 8.0f);
    CHECK(moveFor({0.25f, 0.25f, 0.25f, 0.25f}) == 8.0f);
    CHECK(moveFor({0.5f, 0.5f}) == 8.0f);
    CHECK(moveFor(std::vector<float>(30, 1.0f / 30.0f), 1.0f / 60.0f, 60.0f) == 60.0f);
    CHECK(moveFor(std::vector<float>(144, 1.0f / 144.0f), 1.0f / 60.0f, 60.0f) == 60.0f);
}

TEST_CASE("PhysicsStep caps catch-up work and keeps only fractional time", "[physics][step]")
{
    Ecs ecs;
    auto &physics = ecs.registerSystem<PhysicsStep>(0.25f, 2);
    std::vector<float> ticks;
    std::vector<int> order;
    physics.registerSystem<Recorder>(ticks, order, 1);

    ecs.update(1.125f);
    CHECK(ticks.size() == 2);
    CHECK(physics.alpha() == 0.5f);

    ecs.update(0.125f);
    CHECK(ticks.size() == 3);
    CHECK(physics.alpha() == 0.0f);
}

TEST_CASE("PhysicsStep rejects invalid timing settings", "[physics][step]")
{
    Ecs ecs;
    REQUIRE_THROWS_AS(ecs.registerSystem<PhysicsStep>(0.0f, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(ecs.registerSystem<PhysicsStep>(0.25f, 0), std::invalid_argument);
    ecs.registerSystem<PhysicsStep>(0.25f, 1);
    REQUIRE_THROWS_AS(ecs.update(-0.25f), std::invalid_argument);
}
