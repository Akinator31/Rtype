// Tests of the scenes: the stack of open scenes, and the entities that belong to them.

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace rtype::ecs;
using namespace rtype::engine;

TEST_CASE("The engine destroys the entities of a closed scene and keeps the persistent ones", "[engine][scenes]")
{
    Engine engine;
    Ecs &ecs = engine.getEcs();
    Scenes &scenes = engine.getScenes();
    const SceneId menu = scenes.open();
    const entity_t button = ecs.createEntity();
    ecs.addComponent<SceneMember>(button, menu);
    const entity_t label = ecs.createEntity();
    attach(ecs, label, button);
    ecs.addComponent<SceneMember>(label, menu);
    const entity_t stars = ecs.createEntity();

    // Scenes only keeps the stack: the entities stay until the engine's next update, hidden
    scenes.close();
    REQUIRE_FALSE(scenes.isOpen(menu));
    REQUIRE(ecs.isAlive(button));
    REQUIRE_FALSE(scenes.isVisible(button));
    REQUIRE_FALSE(scenes.updates(button));

    engine.update(0.0f);
    REQUIRE_FALSE(ecs.isAlive(button));
    REQUIRE_FALSE(ecs.isAlive(label));
    REQUIRE(ecs.isAlive(stars));
}

TEST_CASE("Closed scenes are given once, in closing order", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>();
    const SceneId game = scenes.open();
    const SceneId pause = scenes.open();

    scenes.clear();
    REQUIRE(scenes.takeClosedScenes() == std::vector<SceneId>{pause, game});
    REQUIRE(scenes.takeClosedScenes().empty());
}

TEST_CASE("Only the scene at the top receives the input", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>();
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
    Scenes &scenes = ecs.registerSystem<Scenes>();
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
    Scenes &scenes = ecs.registerSystem<Scenes>();
    const SceneId home = scenes.open();
    const SceneId options = scenes.open();

    REQUIRE_FALSE(scenes.isVisible(home));
    REQUIRE_FALSE(scenes.updates(home));
    REQUIRE(scenes.isVisible(options));
}

TEST_CASE("Back calls the scene's action, or closes it", "[engine][scenes]")
{
    Ecs ecs;
    Scenes &scenes = ecs.registerSystem<Scenes>();
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
    Scenes &scenes = ecs.registerSystem<Scenes>();
    const entity_t stars = ecs.createEntity();
    scenes.open();

    REQUIRE(scenes.isVisible(stars));
    REQUIRE(scenes.updates(stars));
    REQUIRE_FALSE(scenes.receivesInput(stars));
}
