// The menus built with entities are tested without a window: fake measures place the widgets, a fake input
// source drives them.
//
// With the fake measures and the default style, a widget is 88 units high (56 of text and 16 of padding above
// and below), with 16 units between widgets. A menu's column is centred at x = 960, y = 604.8: with three
// widgets, their centres are at y = 500.8, 604.8 and 708.8.

#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"

#include "FakeInput.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace rtype::engine;
using rtype::ecs::entity_t;
using rtype::engine::SceneId;
using rtype::input::Key;
using rtype::input::MouseButton;

namespace
{

// Every character is half as wide as the text is high; window pixels are game units
class FakeMetrics : public Metrics
{
public:
    [[nodiscard]] rtype::render::Vector2 measureText(rtype::render::FontId /*font*/, const std::string &text,
                                                     float size) const override
    {
        return {0.5f * size * static_cast<float>(text.size()), size};
    }
    [[nodiscard]] rtype::render::Vector2 gameSize() const override { return {1920.0f, 1080.0f}; }
    [[nodiscard]] rtype::render::Vector2 windowToGame(rtype::render::Vector2 position) const override
    {
        return position;
    }
};

// The systems of the client, in the order they must be registered
struct World {
    FakeMetrics metrics;
    FakeInput input;
    InterfaceStyle style{.font = {0}};
    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    rtype::engine::Scenes &scenes = engine.getScenes();

    World()
    {
        ecs.registerSystem<InterfaceSystem>(ecs, scenes, input, metrics, style);
        ecs.registerSystem<WidgetTextSystem>();
        ecs.registerSystem<TweenSystem>();
        ecs.registerSystem<HitboxSystem>(metrics, style);
        ecs.registerSystem<ColumnLayoutSystem>(ecs);
        ecs.registerSystem<rtype::ecs::TransformSystem>(ecs);
    }

    void update(float elapsed = 0.0f) { engine.update(elapsed); }

    // A quick key press: held for one frame, then released
    void press(std::set<Key> keys)
    {
        input.keys = std::move(keys);
        update();
        input.keys.clear();
        update();
    }

    void clickAt(float x, float y)
    {
        input.mouse = {x, y};
        input.mouseButtons = {MouseButton::Left};
        update();
        input.mouseButtons.clear();
        update();
    }

    [[nodiscard]] bool isFocused(entity_t entity) { return ecs.hasComponent<Focused>(entity); }

    [[nodiscard]] std::string textOf(entity_t entity) { return ecs.getComponent<Text>(entity).text; }
};

class Counter : public rtype::ecs::System
{
public:
    int calls = 0;

    explicit Counter(rtype::ecs::ComponentCaches & /*caches*/) {}

    void update(float /*elapsed*/) override { ++calls; }
};

// Triggers an event every time it runs
class Trigger : public rtype::ecs::System
{
    Engine &_engine;
    std::string _event;

public:
    Trigger(rtype::ecs::ComponentCaches & /*caches*/, Engine &engine, std::string event)
        : _engine(engine), _event(std::move(event))
    {
    }

    void update(float /*elapsed*/) override { _engine.triggerEvent(_event); }
};

} // namespace

TEST_CASE("The first widget that can get the focus has it", "[engine][interface]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t label = menu.label("Choose");
    const entity_t play = menu.button("Play", [] {});

    world.update();

    REQUIRE_FALSE(world.isFocused(label));
    REQUIRE(world.isFocused(play));
}

TEST_CASE("Confirm runs the action of the focused button", "[engine][interface]")
{
    World world;
    std::string pressed;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    menu.button("Play", [&] { pressed = "Play"; });
    menu.button("Quit", [&] { pressed = "Quit"; });
    world.update();

    world.press({Key::Enter});

    REQUIRE(pressed == "Play");
}

TEST_CASE("Up and Down move the focus, skip labels and go around", "[engine][interface]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    menu.label("Not focusable");
    const entity_t quit = menu.button("Quit", [] {});
    world.update();

    world.press({Key::Down});
    REQUIRE(world.isFocused(quit));
    world.press({Key::Down});
    REQUIRE(world.isFocused(play));
    REQUIRE_FALSE(world.isFocused(quit));
    world.press({Key::Up});
    REQUIRE(world.isFocused(quit));
}

TEST_CASE("Back closes the menu, and the game does not see the key", "[engine][interface]")
{
    World world;
    const SceneId game = world.scenes.open({.updatesWhenCovered = true});
    MenuBuilder pause(world.ecs, world.scenes, world.metrics, world.style, "Pause", {.overlay = true});
    pause.button("Resume", [] {});
    world.update();

    world.input.keys = {Key::Escape};
    world.update();
    // Closed at the end of the update: during it, the game did not receive the input
    REQUIRE_FALSE(world.scenes.isOpen(pause.scene()));
    REQUIRE_FALSE(world.scenes.receivesInput(game));

    world.update();
    REQUIRE(world.scenes.receivesInput(game));
}

TEST_CASE("A button can open another menu", "[engine][interface]")
{
    World world;
    rtype::ecs::Ecs &ecs = world.ecs;
    std::vector<SceneId> opened;
    MenuBuilder menu(ecs, world.scenes, world.metrics, world.style, "Menu");
    menu.button("Options", [&] {
        MenuBuilder options(ecs, world.scenes, world.metrics, world.style, "Options");
        options.button("Back", [] {});
        opened.push_back(options.scene());
    });
    world.update();

    world.press({Key::Enter});

    REQUIRE(opened.size() == 1);
    REQUIRE(world.scenes.top() == opened.front());
    world.press({Key::Escape});
    REQUIRE(world.scenes.top() == menu.scene());
}

TEST_CASE("A toggle follows the setting it controls", "[engine][interface]")
{
    World world;
    bool fullscreen = false;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Options");
    const entity_t toggle =
        menu.toggle("Fullscreen", [&] { return fullscreen; }, [&](bool on) { fullscreen = on; });
    world.update();

    world.press({Key::Enter});
    REQUIRE(fullscreen);
    world.press({Key::Left});
    REQUIRE_FALSE(fullscreen);
    world.press({Key::Right});
    REQUIRE(fullscreen);

    // Changed elsewhere, for instance by a shortcut key: the text shows the real value
    fullscreen = false;
    world.update();
    REQUIRE(world.textOf(toggle) == "Fullscreen: Off");
}

TEST_CASE("A slider changes by steps and stays between 0 and 1", "[engine][interface]")
{
    World world;
    float volume = 0.8f;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Options");
    const entity_t slider =
        menu.slider("Volume", [&] { return volume; }, [&](float value) { volume = value; });
    world.update();

    world.press({Key::Right});
    world.press({Key::Right});
    world.press({Key::Right});
    REQUIRE(volume == 1.0f);

    world.press({Key::Left});
    REQUIRE(std::abs(volume - 0.9f) < 1e-6f);
    REQUIRE(world.textOf(slider) == "Volume: 90%");
}

TEST_CASE("A choice goes through its options and around", "[engine][interface]")
{
    World world;
    std::size_t filter = 0;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Options");
    const entity_t choice = menu.choice(
        "Filter", {"None", "Protanopia", "Deuteranopia"}, [&] { return filter; },
        [&](std::size_t index) { filter = index; });
    world.update();

    world.press({Key::Left});
    REQUIRE(filter == 2);
    world.press({Key::Right});
    REQUIRE(filter == 0);
    REQUIRE(world.textOf(choice) == "Filter: None");
}

TEST_CASE("A text field is edited only while it has the focus", "[engine][interface]")
{
    World world;
    std::string submitted;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Options");
    menu.button("Play", [] {});
    const entity_t name = menu.textField("Name", "", [&](const std::string &text) { submitted = text; });
    world.update();

    world.input.typed = U"lost";
    world.update();
    REQUIRE(world.ecs.getComponent<TextField>(name).input.text().empty());

    world.press({Key::Down});
    world.input.typed = U"Pavel";
    world.update();
    world.press({Key::Enter});

    REQUIRE(world.ecs.getComponent<TextField>(name).input.text() == "Pavel");
    REQUIRE(submitted == "Pavel");
    REQUIRE(world.textOf(name) == "Name: Pavel");
}

TEST_CASE("Widgets are stacked in a column, centred", "[engine][layout]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "");
    const entity_t first = menu.button("Play", [] {});
    const entity_t second = menu.button("Options", [] {});
    const entity_t third = menu.button("Quit", [] {});

    world.update();

    const auto y = [&](entity_t entity) { return world.ecs.getComponent<rtype::ecs::Position>(entity).y; };
    REQUIRE(std::abs(y(first) - 500.8f) < 0.01f);
    REQUIRE(std::abs(y(second) - 604.8f) < 0.01f);
    REQUIRE(std::abs(y(third) - 708.8f) < 0.01f);
    REQUIRE(world.ecs.getComponent<rtype::ecs::Position>(second).x == 960.0f);
}

TEST_CASE("The mouse focuses the widget under it, only when it moves", "[engine][interface]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    menu.button("Play", [] {});
    const entity_t options = menu.button("Options", [] {});
    const entity_t quit = menu.button("Quit", [] {});
    world.update();

    world.input.mouse = {960.0f, 604.8f};
    world.update();
    REQUIRE(world.isFocused(options));

    // The keyboard moves the focus away: the still pointer does not take it back
    world.press({Key::Down});
    REQUIRE(world.isFocused(quit));
}

TEST_CASE("A click activates the widget under the pointer, not the focused one", "[engine][interface]")
{
    World world;
    std::string pressed;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    menu.button("Play", [&] { pressed = "Play"; });
    menu.button("Options", [&] { pressed = "Options"; });
    const entity_t quit = menu.button("Quit", [&] { pressed = "Quit"; });
    world.update();

    world.clickAt(100.0f, 100.0f);
    REQUIRE(pressed.empty());

    world.clickAt(960.0f, 708.8f);
    REQUIRE(pressed == "Quit");
    REQUIRE(world.isFocused(quit));
}

TEST_CASE("The mouse wheel adjusts the widget under the pointer", "[engine][interface]")
{
    World world;
    float volume = 0.5f;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Options");
    menu.slider("Volume", [&] { return volume; }, [&](float value) { volume = value; });
    world.update();

    world.input.mouse = {960.0f, 604.8f};
    world.input.wheel = 1.0f;
    world.update();

    REQUIRE(std::abs(volume - 0.6f) < 1e-6f);
}

TEST_CASE("Only the menu at the top receives the input", "[engine][interface]")
{
    World world;
    int presses = 0;
    MenuBuilder below(world.ecs, world.scenes, world.metrics, world.style, "Below");
    below.button("Count", [&] { ++presses; });
    MenuBuilder top(world.ecs, world.scenes, world.metrics, world.style, "Top");
    top.button("Nothing", [] {});
    world.update();

    world.press({Key::Enter});

    REQUIRE(presses == 0);
}

TEST_CASE("The focused widget grows with a bounce, then settles", "[engine][tween]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    world.update();

    float largest = 0.0f;
    for (int frame = 0; frame < 30; ++frame) {
        world.update(0.016f);
        largest = std::max(largest, world.ecs.getComponent<Scale>(play).value);
    }

    REQUIRE(largest > world.style.focusScale);
    REQUIRE(std::abs(world.ecs.getComponent<Scale>(play).value - world.style.focusScale) < 1e-4f);
    REQUIRE_FALSE(world.ecs.hasComponent<Tween>(play));
}

TEST_CASE("Easings start at 0 and end at 1", "[engine][tween]")
{
    for (const Easing easing : {Easing::Linear, Easing::EaseOut, Easing::EaseOutBack}) {
        REQUIRE(std::abs(ease(easing, 0.0f)) < 1e-6f);
        REQUIRE(std::abs(ease(easing, 1.0f) - 1.0f) < 1e-6f);
    }
    REQUIRE(ease(Easing::Linear, 0.25f) == 0.25f);
    REQUIRE(ease(Easing::EaseOut, 0.5f) > 0.5f);
}

TEST_CASE("A back and forth tween never ends", "[engine][tween]")
{
    rtype::ecs::Ecs ecs;
    ecs.registerSystem<TweenSystem>();
    const entity_t light = ecs.createEntity();
    ecs.addComponent<Tween>(light, Tween{.target = TweenTarget::Opacity,
                                         .from = 1.0f,
                                         .to = 0.0f,
                                         .duration = 0.5f,
                                         .easing = Easing::Linear,
                                         .pingPong = true});

    ecs.update(0.5f);
    REQUIRE(std::abs(ecs.getComponent<Opacity>(light).value) < 1e-6f);
    ecs.update(0.25f);
    REQUIRE(std::abs(ecs.getComponent<Opacity>(light).value - 0.5f) < 1e-6f);
    ecs.update(0.25f);
    REQUIRE(std::abs(ecs.getComponent<Opacity>(light).value - 1.0f) < 1e-6f);
    REQUIRE(ecs.hasComponent<Tween>(light));
}

TEST_CASE("A trigger takes effect at the next engine update", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    counter.enabled = false;
    engine.createEvent("start");
    engine.subscribeToEvent("start", counter, Reaction::Enable);

    engine.triggerEvent("start");
    CHECK_FALSE(counter.enabled);
    CHECK(counter.calls == 0);

    engine.update(0.1f);
    CHECK(counter.enabled);
    CHECK(counter.calls == 1);
    engine.update(0.1f);
    CHECK(counter.calls == 2);
}

TEST_CASE("Permanent reactions enable, disable or toggle their systems", "[engine][event]")
{
    Engine engine;
    Counter &enabled = engine.getEcs().registerSystem<Counter>();
    Counter &disabled = engine.getEcs().registerSystem<Counter>();
    Counter &toggled = engine.getEcs().registerSystem<Counter>();
    enabled.enabled = false;
    engine.createEvent("switch");
    engine.subscribeToEvent("switch", enabled, Reaction::Enable);
    engine.subscribeToEvent("switch", disabled, Reaction::Disable);
    engine.subscribeToEvent("switch", toggled, Reaction::Toggle);

    engine.triggerEvent("switch");
    engine.update(0.1f);
    CHECK(enabled.enabled);
    CHECK_FALSE(disabled.enabled);
    CHECK_FALSE(toggled.enabled);

    engine.triggerEvent("switch");
    engine.update(0.1f);
    CHECK(enabled.enabled);
    CHECK_FALSE(disabled.enabled);
    CHECK(toggled.enabled);
    CHECK(enabled.calls == 2);
    CHECK(disabled.calls == 0);
    CHECK(toggled.calls == 1);
}

TEST_CASE("Once reactions last for a single update", "[engine][event]")
{
    Engine engine;
    Counter &enabled = engine.getEcs().registerSystem<Counter>();
    Counter &disabled = engine.getEcs().registerSystem<Counter>();
    Counter &toggled = engine.getEcs().registerSystem<Counter>();
    enabled.enabled = false;
    engine.createEvent("pulse");
    engine.subscribeToEvent("pulse", enabled, Reaction::Enable, Duration::Once);
    engine.subscribeToEvent("pulse", disabled, Reaction::Disable, Duration::Once);
    engine.subscribeToEvent("pulse", toggled, Reaction::Toggle, Duration::Once);

    engine.triggerEvent("pulse");
    engine.update(0.1f);
    CHECK(enabled.calls == 1);
    CHECK(disabled.calls == 0);
    CHECK(toggled.calls == 0);
    CHECK_FALSE(enabled.enabled);
    CHECK(disabled.enabled);
    CHECK(toggled.enabled);

    engine.update(0.1f);
    CHECK(enabled.calls == 1);
    CHECK(disabled.calls == 1);
    CHECK(toggled.calls == 1);
}

TEST_CASE("An event only changes its own subscribers", "[engine][event]")
{
    Engine engine;
    Counter &a = engine.getEcs().registerSystem<Counter>();
    Counter &b = engine.getEcs().registerSystem<Counter>();

    engine.createEvent("shoot");
    engine.createEvent("death");
    engine.subscribeToEvent("shoot", a, Reaction::Disable);
    engine.subscribeToEvent("death", b, Reaction::Disable);
    engine.triggerEvent("shoot");
    engine.update(0.1f);

    CHECK_FALSE(a.enabled);
    CHECK(b.enabled);
}

TEST_CASE("An unsubscribed system is not changed", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    Event &event = engine.createEvent("stop");
    event.addSubscriber(counter, Reaction::Disable);
    event.removeSubscriber(counter);

    engine.triggerEvent("stop");
    engine.update(0.1f);

    CHECK(counter.enabled);
    CHECK(event.getSubscriptions().empty());
}

TEST_CASE("Reactions are applied in the order of the triggers", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    engine.createEvent("on");
    engine.createEvent("off");
    engine.createEvent("flip");
    engine.subscribeToEvent("on", counter, Reaction::Enable);
    engine.subscribeToEvent("off", counter, Reaction::Disable);
    engine.subscribeToEvent("flip", counter, Reaction::Toggle);

    engine.triggerEvent("on");
    engine.triggerEvent("off");
    engine.update(0.1f);
    CHECK_FALSE(counter.enabled);

    engine.triggerEvent("off");
    engine.triggerEvent("on");
    engine.update(0.1f);
    CHECK(counter.enabled);

    engine.triggerEvent("flip");
    engine.triggerEvent("flip");
    engine.update(0.1f);
    CHECK(counter.enabled);
}

TEST_CASE("A permanent reaction cancels the restore of a once reaction", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    counter.enabled = false;
    engine.createEvent("pulse");
    engine.createEvent("start");
    engine.subscribeToEvent("pulse", counter, Reaction::Enable, Duration::Once);
    engine.subscribeToEvent("start", counter, Reaction::Enable);

    engine.triggerEvent("pulse");
    engine.triggerEvent("start");
    engine.update(0.1f);
    engine.update(0.1f);

    CHECK(counter.enabled);
    CHECK(counter.calls == 2);
}

TEST_CASE("Several once reactions restore the state before the first one", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    counter.enabled = false;
    engine.createEvent("pulse");
    engine.createEvent("flip");
    engine.subscribeToEvent("pulse", counter, Reaction::Enable, Duration::Once);
    engine.subscribeToEvent("flip", counter, Reaction::Toggle, Duration::Once);

    engine.triggerEvent("pulse");
    engine.triggerEvent("flip");
    engine.triggerEvent("flip");
    engine.update(0.1f);

    CHECK(counter.calls == 1);
    CHECK_FALSE(counter.enabled);
}

TEST_CASE("A once reaction runs a system registered before the trigger", "[engine][event]")
{
    Engine engine;
    Counter &counter = engine.getEcs().registerSystem<Counter>();
    counter.enabled = false;
    Trigger &trigger = engine.getEcs().registerSystem<Trigger>(engine, "shoot");
    engine.createEvent("shoot");
    engine.subscribeToEvent("shoot", counter, Reaction::Enable, Duration::Once);

    engine.update(0.1f);
    CHECK(counter.calls == 0);
    trigger.enabled = false;
    engine.update(0.1f);
    CHECK(counter.calls == 1);
    engine.update(0.1f);
    CHECK(counter.calls == 1);
    CHECK_FALSE(counter.enabled);
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
    Counter &counter = engine.getEcs().registerSystem<Counter>();

    Event &event = engine.createEvent("stop");
    engine.subscribeToEvent("stop", counter, Reaction::Disable);

    CHECK(&engine.createEvent("stop") == &event);
    engine.triggerEvent("stop");
    engine.update(0.1f);
    CHECK_FALSE(counter.enabled);
}

namespace
{

// A scene with a single entity, recorded in `entities` each time the scene opens
void createRecordedScene(Engine &engine, const std::string &name, std::vector<entity_t> &entities,
                         SceneSettings settings = {})
{
    engine.createScene(name, std::move(settings), [&entities](Engine &owner, SceneId scene) {
        const entity_t entity = owner.getEcs().createEntity();
        owner.getEcs().addComponent<SceneMember>(entity, scene);
        entities.push_back(entity);
    });
}

} // namespace

TEST_CASE("A scene switch takes effect at the next engine update", "[engine][scenes]")
{
    Engine engine;
    std::vector<entity_t> entities;
    createRecordedScene(engine, "menu", entities);

    engine.switchScene("menu");
    CHECK_FALSE(engine.getScenes().top());
    CHECK(entities.empty());

    engine.update(0.1f);
    REQUIRE(entities.size() == 1);
    REQUIRE(engine.getScenes().top());
    CHECK(engine.getScenes().sceneOf(entities[0]) == engine.getScenes().top());
}

TEST_CASE("A scene switch closes every open scene and keeps the persistent entities", "[engine][scenes]")
{
    Engine engine;
    std::vector<entity_t> games;
    std::vector<entity_t> pauses;
    std::vector<entity_t> menus;
    createRecordedScene(engine, "game", games);
    createRecordedScene(engine, "pause", pauses, {.overlay = true});
    createRecordedScene(engine, "menu", menus);
    const entity_t stars = engine.getEcs().createEntity();

    engine.switchScene("game");
    engine.pushScene("pause");
    engine.update(0.1f);
    const SceneId game = *engine.getScenes().sceneOf(games[0]);
    const SceneId pause = *engine.getScenes().sceneOf(pauses[0]);
    CHECK(engine.getScenes().top() == pause);
    CHECK(engine.getScenes().isVisible(game));

    engine.switchScene("menu");
    engine.update(0.1f);
    CHECK_FALSE(engine.getScenes().isOpen(game));
    CHECK_FALSE(engine.getScenes().isOpen(pause));
    // Their ids may be recycled by the menu: what matters is that nothing belongs to them anymore
    CHECK(engine.getScenes().sceneOf(games[0]) != game);
    CHECK(engine.getScenes().sceneOf(pauses[0]) != pause);
    CHECK(engine.getEcs().isAlive(menus[0]));
    CHECK(engine.getEcs().isAlive(stars));
}

TEST_CASE("Pushing a scene opens it over the others, popping closes the top one", "[engine][scenes]")
{
    Engine engine;
    std::vector<entity_t> games;
    std::vector<entity_t> pauses;
    createRecordedScene(engine, "game", games);
    createRecordedScene(engine, "pause", pauses);

    engine.switchScene("game");
    engine.update(0.1f);
    const SceneId game = *engine.getScenes().top();

    engine.pushScene("pause");
    engine.update(0.1f);
    CHECK(engine.getScenes().isOpen(game));
    CHECK(engine.getScenes().top() != game);

    engine.popScene();
    engine.update(0.1f);
    CHECK(engine.getScenes().top() == game);
    CHECK_FALSE(engine.getEcs().isAlive(pauses[0]));
    CHECK(engine.getEcs().isAlive(games[0]));

    engine.popScene();
    engine.popScene();
    engine.update(0.1f);
    CHECK_FALSE(engine.getScenes().top());
}

TEST_CASE("A named scene triggers its events when it opens and closes", "[engine][scenes]")
{
    Engine engine;
    Counter &physics = engine.getEcs().registerSystem<Counter>();
    physics.enabled = false;
    engine.createScene("game");
    engine.createScene("menu");
    engine.subscribeToEvent("game:open", physics, Reaction::Enable);
    engine.subscribeToEvent("game:close", physics, Reaction::Disable);

    // The reactions apply in the update that opens the scene
    engine.switchScene("game");
    engine.update(0.1f);
    CHECK(physics.enabled);
    CHECK(physics.calls == 1);

    engine.switchScene("menu");
    engine.update(0.1f);
    CHECK_FALSE(physics.enabled);
    CHECK(physics.calls == 1);
}

TEST_CASE("A named scene closed without the engine triggers its close event at the next update",
          "[engine][scenes]")
{
    Engine engine;
    Counter &physics = engine.getEcs().registerSystem<Counter>();
    engine.createScene("game");
    engine.subscribeToEvent("game:close", physics, Reaction::Disable);
    engine.switchScene("game");
    engine.update(0.1f);

    // Like a menu button calling Scenes::close()
    engine.getScenes().close();
    CHECK(physics.enabled);
    engine.update(0.1f);
    CHECK_FALSE(physics.enabled);
}

TEST_CASE("A scene exists only once created, and is never redefined", "[engine][scenes]")
{
    Engine engine;
    CHECK_FALSE(engine.hasScene("game"));
    CHECK_THROWS_AS(engine.switchScene("game"), std::out_of_range);
    CHECK_THROWS_AS(engine.pushScene("game"), std::out_of_range);

    int builds = 0;
    int otherBuilds = 0;
    engine.createScene("game", {}, [&builds](Engine & /*engine*/, SceneId /*scene*/) { ++builds; });
    engine.createScene("game", {}, [&otherBuilds](Engine & /*engine*/, SceneId /*scene*/) { ++otherBuilds; });
    CHECK(engine.hasScene("game"));
    CHECK(engine.hasEvent("game:open"));
    CHECK(engine.hasEvent("game:close"));

    engine.switchScene("game");
    engine.update(0.1f);
    CHECK(builds == 1);
    CHECK(otherBuilds == 0);
}
