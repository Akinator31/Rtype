// Tests of the scenes: the stack of open scenes, and the entities that belong to them.

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rtype::ecs;
using namespace rtype::engine;

TEST_CASE("Closing a scene destroys its entities and keeps the persistent ones", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    const SceneId menu = scenes.open();
    const entity_t button = ecs.createEntity();
    ecs.addComponent<SceneMember>(button, menu);
    const entity_t label = ecs.createEntity();
    attach(ecs, label, button);
    ecs.addComponent<SceneMember>(label, menu);
    const entity_t stars = ecs.createEntity();

    scenes.close();

    REQUIRE_FALSE(ecs.isAlive(button));
    REQUIRE_FALSE(ecs.isAlive(label));
    REQUIRE(ecs.isAlive(stars));
    REQUIRE_FALSE(scenes.isOpen(menu));
}

TEST_CASE("Only the scene at the top receives the input", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    const SceneId game = scenes.open({.updatesWhenCovered = true});
    ecs.update(0.0f);
    REQUIRE(scenes.receivesInput(game));

    const SceneId pause = scenes.open({.overlay = true});
    ecs.update(0.0f);

    REQUIRE(scenes.receivesInput(pause));
    REQUIRE_FALSE(scenes.receivesInput(game));
    REQUIRE(scenes.updates(game));
    REQUIRE(scenes.isVisible(game));
}

TEST_CASE("The scene closed during an update keeps the input until the next one", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    const SceneId game = scenes.open();
    scenes.open({.overlay = true});
    ecs.update(0.0f);

    // Escape closes the pause menu: later in the same update, the game must not read Escape too
    scenes.close();
    REQUIRE_FALSE(scenes.receivesInput(game));

    ecs.update(0.0f);
    REQUIRE(scenes.receivesInput(game));
}

TEST_CASE("A scene that is not an overlay hides and pauses the ones below", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    const SceneId home = scenes.open();
    const SceneId options = scenes.open();

    REQUIRE_FALSE(scenes.isVisible(home));
    REQUIRE_FALSE(scenes.updates(home));
    REQUIRE(scenes.isVisible(options));
}

TEST_CASE("Back calls the scene's action, or closes it", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    int backs = 0;
    const SceneId game = scenes.open({.onBack = [&] { ++backs; }});
    const SceneId pause = scenes.open();

    scenes.back();
    REQUIRE_FALSE(scenes.isOpen(pause));

    scenes.back();
    REQUIRE(backs == 1);
    REQUIRE(scenes.isOpen(game));
}

TEST_CASE("Persistent entities are visible and update in every scene", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>(ecs);
    const entity_t stars = ecs.createEntity();
    scenes.open();

    REQUIRE(scenes.isVisible(stars));
    REQUIRE(scenes.updates(stars));
    REQUIRE_FALSE(scenes.receivesInput(stars));
}
