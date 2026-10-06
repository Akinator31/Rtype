// Tests of the Entity Component System (libs/ecs).

#include "rtype/ecs/Ecs.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The ECS library greets", "[ecs]")
{
    REQUIRE(rtype::ecs::helloWorld() == "Hello from the ECS!");
}
