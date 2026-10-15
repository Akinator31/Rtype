#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Rendering/ColliderDebugRenderer.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/physics/Component/Collider.hpp"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

using rtype::ecs::Ecs;
using rtype::ecs::Hidden;
using rtype::ecs::Position;
using rtype::engine::ColliderDebugRenderer;
using rtype::engine::SceneMember;
using rtype::engine::Scenes;
using rtype::physics::Aabb;
using rtype::physics::Collider;
using rtype::physics::Sphere;
using rtype::physics::Vec3;

TEST_CASE("Collider debug geometry uses world positions and shape offsets", "[engine][debug-collider]")
{
    Ecs ecs;
    auto &scenes = ecs.registerSystem<Scenes>();
    const auto scene = scenes.open();
    ColliderDebugRenderer debug(ecs, scenes, 64.0f);

    const auto box = ecs.createEntity();
    ecs.addComponent<SceneMember>(box, scene);
    ecs.addComponent<Position>(box, 10.0f, 20.0f);
    ecs.addComponent<Collider>(box, Aabb{{2.0f, 3.0f, 0.0f}, {4.0f, 5.0f, 0.0f}},
                               Vec3{6.0f, 7.0f, 0.0f});

    const auto circle = ecs.createEntity();
    ecs.addComponent<SceneMember>(circle, scene);
    ecs.addComponent<Position>(circle, 30.0f, 40.0f);
    ecs.addComponent<Collider>(circle, Sphere{{1.0f, 2.0f, 0.0f}, 3.0f},
                               Vec3{4.0f, 5.0f, 0.0f}, true);

    const auto shapes = debug.colliders();
    REQUIRE(shapes.size() == 2);
    CHECK(shapes[0].entity == box);
    CHECK(shapes[0].center.x == 18.0f);
    CHECK(shapes[0].center.y == 30.0f);
    CHECK(shapes[0].halfSize.x == 4.0f);
    CHECK(shapes[0].halfSize.y == 5.0f);
    CHECK_FALSE(shapes[0].circle);
    CHECK_FALSE(shapes[0].trigger);
    CHECK(shapes[1].entity == circle);
    CHECK(shapes[1].center.x == 35.0f);
    CHECK(shapes[1].center.y == 47.0f);
    CHECK(shapes[1].radius == 3.0f);
    CHECK(shapes[1].circle);
    CHECK(shapes[1].trigger);
}

TEST_CASE("Collider debug hides entities outside visible scenes", "[engine][debug-collider]")
{
    Ecs ecs;
    auto &scenes = ecs.registerSystem<Scenes>();
    const auto game = scenes.open();
    ColliderDebugRenderer debug(ecs, scenes, 64.0f);

    const auto visible = ecs.createEntity();
    ecs.addComponent<SceneMember>(visible, game);
    ecs.addComponent<Position>(visible);
    ecs.addComponent<Collider>(visible, Aabb{{}, {1.0f, 1.0f, 0.0f}});

    const auto hidden = ecs.createEntity();
    ecs.addComponent<SceneMember>(hidden, game);
    ecs.addComponent<Position>(hidden);
    ecs.addComponent<Collider>(hidden, Aabb{{}, {1.0f, 1.0f, 0.0f}});
    ecs.addComponent<Hidden>(hidden);

    REQUIRE(debug.colliders().size() == 1);
    CHECK(debug.colliders().front().entity == visible);

    scenes.open();
    CHECK(debug.colliders().empty());
}

TEST_CASE("Collider debug grid follows configured cell boundaries", "[engine][debug-collider]")
{
    Ecs ecs;
    auto &scenes = ecs.registerSystem<Scenes>();
    ColliderDebugRenderer debug(ecs, scenes, 64.0f);

    const auto lines = debug.gridLines({130.0f, 130.0f});
    REQUIRE(lines.size() == 6);
    CHECK(lines[0].start.x == 0.0f);
    CHECK(lines[1].start.x == 64.0f);
    CHECK(lines[2].start.x == 128.0f);
    CHECK(lines[3].start.y == 0.0f);
    CHECK(lines[4].start.y == 64.0f);
    CHECK(lines[5].start.y == 128.0f);
    CHECK_THROWS_AS(ColliderDebugRenderer(ecs, scenes, 0.0f), std::invalid_argument);
}
