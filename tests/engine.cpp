#include "Engine/Engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using namespace ecs;
using namespace engine;

namespace
{

class Counter : public System
{
public:
    int calls = 0;

    explicit Counter(ComponentCaches & /*caches*/) {}

    void update() override { ++calls; }
};

} // namespace

TEST_CASE("Triggering an event updates its subscribers", "[engine][event]")
{
    Engine engine;
    Counter &a = engine.getEcs().registerSystem<Counter>();
    Counter &b = engine.getEcs().registerSystem<Counter>();

    engine.createEvent("collision");
    engine.subscribeToEvent("collision", a);
    engine.subscribeToEvent("collision", b);
    engine.triggerEvent("collision");
    engine.triggerEvent("collision");

    CHECK(a.calls == 2);
    CHECK(b.calls == 2);
}

TEST_CASE("An event only updates its own subscribers", "[engine][event]")
{
    Engine engine;
    Counter &a = engine.getEcs().registerSystem<Counter>();
    Counter &b = engine.getEcs().registerSystem<Counter>();

    engine.createEvent("shoot");
    engine.createEvent("death");
    engine.subscribeToEvent("shoot", a);
    engine.subscribeToEvent("death", b);
    engine.triggerEvent("shoot");

    CHECK(a.calls == 1);
    CHECK(b.calls == 0);
}

TEST_CASE("Disabled and unsubscribed systems are not updated", "[engine][event]")
{
    Engine engine;
    Counter &a = engine.getEcs().registerSystem<Counter>();
    Counter &b = engine.getEcs().registerSystem<Counter>();
    Event &event = engine.createEvent("tick");
    event.addSubscriber(a);
    event.addSubscriber(b);

    a.enabled = false;
    event.removeSubscriber(b);
    event.trigger();

    CHECK(a.calls == 0);
    CHECK(b.calls == 0);
}

TEST_CASE("An event exists only once created", "[engine][event]")
{
    Engine engine;

    CHECK_FALSE(engine.hasEvent("start"));
    CHECK_THROWS_AS(engine.getEvent("start"), std::out_of_range);
    engine.triggerEvent("start");
    CHECK_FALSE(engine.hasEvent("start"));

    Event &event = engine.createEvent("start");
    CHECK(engine.hasEvent("start"));
    CHECK(event.getName() == "start");
    CHECK(&engine.getEvent("start") == &event);
}

TEST_CASE("Creating an existing event keeps it unchanged", "[engine][event]")
{
    Engine engine;
    Counter &a = engine.getEcs().registerSystem<Counter>();

    Event &event = engine.createEvent("start");
    engine.subscribeToEvent("start", a);

    CHECK(&engine.createEvent("start") == &event);
    engine.triggerEvent("start");
    CHECK(a.calls == 1);
}
