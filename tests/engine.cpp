// The menus built with entities are tested without a window: fake measures place the widgets, a fake input
// source drives them.
//
// With the fake measures and the default style, a widget is 88 units high (56 of text and 16 of padding above
// and below), with 16 units between widgets. A menu's column is centred at x = 960, y = 604.8: with three
// widgets, their centres are at y = 500.8, 604.8 and 708.8.

#include "rtype/engine/Engine.hpp"

#include "FakeInput.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/Scenes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace rtype::engine;
using rtype::ecs::entity_t;
using rtype::ecs::SceneId;
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
    rtype::ecs::Ecs ecs;
    rtype::ecs::Scenes &scenes = ecs.registerSystem<rtype::ecs::Scenes>(ecs);

    World()
    {
        ecs.registerSystem<InterfaceSystem>(ecs, scenes, input, metrics, style);
        ecs.registerSystem<WidgetTextSystem>();
        ecs.registerSystem<TweenSystem>();
        ecs.registerSystem<HitboxSystem>(metrics, style);
        ecs.registerSystem<ColumnLayoutSystem>(ecs);
        ecs.registerSystem<rtype::ecs::TransformSystem>(ecs);
    }

    void update(float elapsed = 0.0f) { ecs.update(elapsed); }

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
