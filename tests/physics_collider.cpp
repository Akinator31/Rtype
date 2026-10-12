#include "rtype/ecs/Ecs.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <variant>

using rtype::ecs::Ecs;
using rtype::physics::Aabb;
using rtype::physics::Collider;
using rtype::physics::Sphere;
using rtype::physics::Vec3;

TEST_CASE("Collider stores an AABB independently of rendering", "[physics][collider]")
{
    Ecs ecs;
    const auto entity = ecs.createEntity();
    ecs.addComponent<Collider>(entity, Aabb{{}, {2.0f, 3.0f, 0.0f}}, Vec3{1.0f, -2.0f, 0.0f}, true);

    const Collider &collider = ecs.getComponent<Collider>(entity);
    REQUIRE(std::holds_alternative<Aabb>(collider.shape));
    const Aabb &box = std::get<Aabb>(collider.shape);
    CHECK(box.center.x == 0.0f);
    CHECK(box.center.y == 0.0f);
    CHECK(box.halfSize.x == 2.0f);
    CHECK(box.halfSize.y == 3.0f);
    CHECK(box.halfSize.z == 0.0f);
    CHECK(collider.offset.x == 1.0f);
    CHECK(collider.offset.y == -2.0f);
    CHECK(collider.trigger);
}

TEST_CASE("Collider defaults to a solid AABB and can hold another shape", "[physics][collider]")
{
    Collider collider;
    CHECK(std::holds_alternative<Aabb>(collider.shape));
    CHECK_FALSE(collider.trigger);
    CHECK(collider.offset.x == 0.0f);

    collider.shape = Sphere{{}, 4.0f};
    REQUIRE(std::holds_alternative<Sphere>(collider.shape));
    CHECK(std::get<Sphere>(collider.shape).radius == 4.0f);
}
