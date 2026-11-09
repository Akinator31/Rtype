// The menus built with entities are tested without a window: fake measures place the widgets, a fake input
// source drives them.
//
// With the fake measures and the default style, a widget is 88 units high (56 of text and 16 of padding above
// and below), with 16 units between widgets. A menu's column is centred at x = 960, y = 604.8: with three
// widgets, their centres are at y = 500.8, 604.8 and 708.8. Buttons are the size of their panel, 476 x 65.

#include "rtype/engine/Engine.hpp"

#include "FakeInput.hpp"
#include "TempDirectory.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Scene/Scenes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <fstream>
#include <optional>
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
                                                     float size, float spacing) const override
    {
        const auto count = static_cast<float>(text.size());
        return {(0.5f * size * count) + (spacing * std::max(count - 1.0f, 0.0f)), size};
    }
    [[nodiscard]] rtype::render::Vector2 gameSize() const override { return {1920.0f, 1080.0f}; }
    [[nodiscard]] rtype::render::Rectangle visibleArea() const override { return screen; }

    // A screen of the shape of the game by default
    rtype::render::Rectangle screen{0.0f, 0.0f, 1920.0f, 1080.0f};
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
        ecs.registerSystem<BoundTextSystem>();
        ecs.registerSystem<TweenSystem>();
        ecs.registerSystem<HitboxSystem>(metrics, style);
        ecs.registerSystem<ColumnLayoutSystem>(ecs);
        ecs.registerSystem<AnchorSystem>(metrics);
        ecs.registerSystem<ToastSystem>(ecs, metrics, style);
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
    world.update(world.style.flare.pressDelay);

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

TEST_CASE("Back works in a scene without widgets, such as the game", "[engine][interface]")
{
    World world;
    int pauses = 0;
    // Only a HUD, nothing to focus: Escape must still reach the scene
    world.scenes.open({.updatesWhenCovered = true, .onBack = [&] { ++pauses; }});
    world.update();

    world.press({Key::Escape});

    REQUIRE(pauses == 1);
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
    world.update(world.style.flare.pressDelay);

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

TEST_CASE("A slider with a rail follows the pointer while it is dragged", "[engine][interface][slider]")
{
    World world;
    float volume = 0.5f;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t slider = ui.slider(
        "Volume", [&] { return volume; }, [&](float value) { volume = value; }, 0.1f,
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    const rtype::ecs::Position rail = world.ecs.getComponent<rtype::ecs::Position>(slider);
    const float left = world.style.slider.getRailLeft(rail.x);
    const float width = world.style.slider.getRailWidth();
    const SliderTrack &track = world.ecs.getComponent<SliderTrack>(slider);

    // Pressed a quarter along the rail: the value goes there, and follows the pointer until it is released
    world.input.mouse = {left + (0.25f * width), world.style.slider.getRailY(rail.y)};
    world.input.mouseButtons = {MouseButton::Left};
    world.update();
    world.update(world.style.slider.fadeDuration);
    CHECK(std::abs(volume - 0.25f) < 0.01f);
    CHECK(track.grab > 0.99f);

    world.input.mouse = {left + (0.8f * width), rail.y};
    world.update();
    CHECK(std::abs(volume - 0.8f) < 0.01f);

    // Past the end of the rail, the value stops at 1; once released, the pointer does not move it any more
    world.input.mouse = {left + (2.0f * width), rail.y};
    world.update();
    CHECK(volume == 1.0f);
    world.input.mouseButtons.clear();
    world.update();
    world.input.mouse = {left, rail.y};
    world.update(world.style.slider.fadeDuration);
    CHECK(volume == 1.0f);
    CHECK(track.grab < 0.01f);
}

TEST_CASE("The keys and the wheel step a slider with a rail", "[engine][interface][slider]")
{
    World world;
    float volume = 0.5f;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    ui.slider(
        "Volume", [&] { return volume; }, [&](float value) { volume = value; }, 0.25f,
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    world.press({Key::Right});
    CHECK(std::abs(volume - 0.75f) < 1e-6f);
    world.press({Key::Left});
    world.press({Key::Left});
    CHECK(std::abs(volume - 0.25f) < 1e-6f);
}

TEST_CASE("A slider with a rail is as big as its row", "[engine][interface][slider]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t slider =
        ui.slider("Volume", [] { return 0.5f; }, [](float) {}, 0.1f, {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    const Hitbox &box = world.ecs.getComponent<Hitbox>(slider);
    CHECK(box.width == world.style.slider.getAreaWidth());
    CHECK(box.height == world.style.slider.getHeight());
}

TEST_CASE("A narrower slider keeps its rail under its own row", "[engine][interface][slider]")
{
    World world;
    float volume = 0.5f;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t slider = ui.slider(
        "Volume", [&] { return volume; }, [&](float value) { volume = value; }, 0.1f,
        {AnchorPoint::TopLeft, 100.0f, 100.0f}, 500.0f);
    world.update();

    const SliderStyle &style = world.style.slider;
    CHECK(world.ecs.getComponent<Hitbox>(slider).width == 500.0f);
    // Dragged at the end of its own rail, not of the rail of the style
    const rtype::ecs::Position row = world.ecs.getComponent<rtype::ecs::Position>(slider);
    const float right = style.getRailLeft(row.x, 500.0f) + style.getRailWidth(500.0f);
    world.input.mouse = {right, style.getRailY(row.y)};
    world.input.mouseButtons = {MouseButton::Left};
    world.update();
    world.update();
    CHECK(volume == 1.0f);
}

TEST_CASE("A row of settings goes through its values with the keys, the wheel and a click",
          "[engine][interface]")
{
    World world;
    int value = 0;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t row = ui.settingRow(
        "Resolution", [&] { return std::to_string(value); }, [&](int direction) { value += direction; },
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    const Hitbox &box = world.ecs.getComponent<Hitbox>(row);
    CHECK(box.width == world.style.setting.width);
    CHECK(box.height == world.style.setting.height);
    CHECK(world.isFocused(row));

    world.press({Key::Right});
    CHECK(value == 1);
    world.press({Key::Left});
    world.press({Key::Left});
    CHECK(value == -1);
    world.press({Key::Enter});
    CHECK(value == 0);

    const rtype::ecs::Position centre = world.ecs.getComponent<rtype::ecs::Position>(row);
    world.clickAt(centre.x, centre.y);
    CHECK(value == 1);
    world.input.wheel = -1.0f;
    world.update();
    world.input.wheel = 0.0f;
    CHECK(value == 0);
    // Its caption stays its caption: the value is drawn apart
    CHECK(world.textOf(row) == "Resolution");
}

TEST_CASE("A row that opens something is pressed as a button", "[engine][interface]")
{
    World world;
    bool opened = false;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t row =
        ui.linkRow("Controls", [&] { opened = true; }, {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    // It has no values to go through
    world.press({Key::Right});
    world.update(world.style.flare.pressDelay);
    CHECK_FALSE(opened);
    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(opened);
    CHECK(world.ecs.getComponent<Hitbox>(row).height == world.style.setting.height);
}

TEST_CASE("A row of the controls is as wide as its table and pressed as a button", "[engine][interface]")
{
    World world;
    bool chosen = false;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t row = ui.keyRow("Fire",
                                   KeyRow{.keys = [] { return std::vector<KeyCap>{{"Space"}}; },
                                          .buttons = [] { return std::vector<std::string>{"A"}; },
                                          .width = 1200.0f,
                                          .keysColumn = 500.0f,
                                          .buttonsColumn = 900.0f},
                                   [&] { chosen = true; }, {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();

    const Hitbox &box = world.ecs.getComponent<Hitbox>(row);
    CHECK(box.width == 1200.0f);
    CHECK(box.height == world.style.keys.rowHeight);
    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(chosen);
}

TEST_CASE("A title is a text drawn in chrome, and follows its function", "[engine][interface]")
{
    World world;
    std::string title = "Settings";
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t entity = ui.boundTitle([&] { return title; }, {AnchorPoint::TopLeft, 0.0f, 0.0f});
    world.update();
    CHECK(world.ecs.hasComponent<Title>(entity));
    CHECK(world.textOf(entity) == "Settings");

    title = "Paramètres";
    world.update();
    CHECK(world.textOf(entity) == "Paramètres");
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

TEST_CASE("A password field shows a mask, one per character, never its text", "[engine][interface]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Sign in");
    const entity_t password = menu.passwordField("Password");
    world.update();
    REQUIRE(world.isFocused(password));

    world.input.typed = U"s\u00e9cret";
    world.update();
    const TextField &field = world.ecs.getComponent<TextField>(password);
    CHECK(field.input.text() == "s\xC3\xA9"
                                "cret");
    CHECK(world.textOf(password) == "Password: ******");
    // The cursor and the selection are measured on the mask: "\xC3\xA9" is one character
    CHECK(fieldContent(field, 3) == "**");
    CHECK(fieldContent(field) == "******");
    CHECK(field.input.isSecret());
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
    REQUIRE(std::abs(y(first) - 532.8f) < 0.01f);
    REQUIRE(std::abs(y(second) - 604.8f) < 0.01f);
    REQUIRE(std::abs(y(third) - 676.8f) < 0.01f);
    REQUIRE(world.ecs.getComponent<rtype::ecs::Position>(second).x == 960.0f);
}

TEST_CASE("A menu can be built in a named scene", "[engine][interface][scenes]")
{
    World world;
    bool played = false;
    entity_t play = 0;
    world.engine.createScene("main_menu", {}, [&](Engine &engine, SceneId scene) {
        MenuBuilder menu(engine.getEcs(), scene, world.metrics, world.style, "R-Type");
        play = menu.button("Play", [&played] { played = true; });
    });

    world.engine.switchScene("main_menu");
    world.update();
    REQUIRE(world.isFocused(play));
    REQUIRE(world.scenes.sceneOf(play) == world.scenes.top());

    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    REQUIRE(played);
}

namespace
{

[[nodiscard]] rtype::ecs::Position positionOf(World &world, entity_t entity)
{
    return world.ecs.getComponent<rtype::ecs::Position>(entity);
}

} // namespace

TEST_CASE("An anchored element is placed from its size and the size of the game", "[engine][layout]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t topLeft = ui.text("Life", {AnchorPoint::TopLeft, 20.0f, 10.0f});
    const entity_t bottomRight = ui.text("Life", {AnchorPoint::BottomRight, -20.0f, -10.0f});
    const entity_t centre = ui.box(100.0f, 50.0f, rtype::render::colors::Red, {});
    const entity_t right = ui.box(100.0f, 50.0f, rtype::render::colors::Red, {AnchorPoint::Right});

    world.update();

    CHECK(positionOf(world, topLeft).x == 20.0f + 56.0f);
    CHECK(positionOf(world, topLeft).y == 10.0f + 28.0f);
    CHECK(positionOf(world, bottomRight).x == 1920.0f - 20.0f - 56.0f);
    CHECK(positionOf(world, bottomRight).y == 1080.0f - 10.0f - 28.0f);
    CHECK(positionOf(world, centre).x == 960.0f);
    CHECK(positionOf(world, centre).y == 540.0f);
    CHECK(positionOf(world, right).x == 1920.0f - 50.0f);
    CHECK(positionOf(world, right).y == 540.0f);
}

TEST_CASE("An anchored element follows the edges of a screen of another shape", "[engine][layout]")
{
    World world;
    // A 16:10 screen shows 60 game units more above and below the game
    world.metrics.screen = {0.0f, -60.0f, 1920.0f, 1200.0f};
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t topLeft = ui.text("Life", {AnchorPoint::TopLeft, 20.0f, 10.0f});
    const entity_t bottomRight = ui.text("Life", {AnchorPoint::BottomRight, -20.0f, -10.0f});
    const entity_t centre = ui.box(100.0f, 50.0f, rtype::render::colors::Red, {});

    world.update();

    CHECK(positionOf(world, topLeft).y == -60.0f + 10.0f + 28.0f);
    CHECK(positionOf(world, bottomRight).y == 1140.0f - 10.0f - 28.0f);
    CHECK(positionOf(world, centre).x == 960.0f);
    CHECK(positionOf(world, centre).y == 540.0f);
}

TEST_CASE("An anchored element follows the size of its text", "[engine][layout]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    std::string name = "Bob";
    const entity_t label = ui.boundText([&name] { return name; }, {AnchorPoint::TopRight});

    world.update();
    CHECK(positionOf(world, label).x == 1920.0f - 42.0f);

    name = "Bobby";
    world.update();
    CHECK(positionOf(world, label).x == 1920.0f - 70.0f);
}

TEST_CASE("A bound text follows its function", "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int score = 0;
    const entity_t label = ui.boundText([&score] { return "Score: " + std::to_string(score); }, {});
    CHECK(world.textOf(label) == "Score: 0");

    score = 42;
    world.update();
    REQUIRE(world.textOf(label) == "Score: 42");
}

TEST_CASE("An anchored button can be focused and pressed", "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int pressed = 0;
    const entity_t back =
        ui.button("Back", [&pressed] { ++pressed; }, {AnchorPoint::BottomLeft, 20.0f, -20.0f});
    const entity_t next = ui.button("Next", [] {}, {AnchorPoint::BottomRight, -20.0f, -20.0f});

    world.update();
    REQUIRE(world.isFocused(back));
    CHECK(positionOf(world, back).x == 20.0f + (world.ecs.getComponent<Hitbox>(back).baseWidth / 2.0f));

    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(pressed == 1);
    world.press({Key::Down});
    CHECK(world.isFocused(next));
}

TEST_CASE("A back button is a button with a chevron", "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int pressed = 0;
    const entity_t back = ui.backButton([&pressed] { ++pressed; }, {AnchorPoint::TopLeft, 20.0f, 20.0f});

    world.update();
    CHECK(world.textOf(back) == "<");
    CHECK(positionOf(world, back).x == 20.0f + (world.ecs.getComponent<Hitbox>(back).baseWidth / 2.0f));
    REQUIRE(world.isFocused(back));

    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(pressed == 1);
}

TEST_CASE("A flare button is as wide as its spaced text and its star", "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t play = ui.button("PLAY", [] {}, {AnchorPoint::TopLeft, 20.0f, 20.0f});

    world.update();
    const FlareStyle &style = world.style.flare;
    // 4 letters half as wide as high, and 3 spaces between them
    const float textWidth = (4.0f * style.textSize / 2.0f) + (3.0f * style.letterSpacing);
    const Hitbox &hitbox = world.ecs.getComponent<Hitbox>(play);
    CHECK(hitbox.baseWidth == style.computeWidth(textWidth));
    CHECK(hitbox.baseHeight == style.height);
    CHECK(positionOf(world, play).x == 20.0f + (hitbox.baseWidth / 2.0f));
}

TEST_CASE("A flare button runs its action once its burst has been seen, once per press",
          "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int pressed = 0;
    ui.button("PLAY", [&pressed] { ++pressed; }, {AnchorPoint::TopLeft, 20.0f, 20.0f});
    world.update();

    world.press({Key::Enter});
    world.press({Key::Enter});
    CHECK(pressed == 0);

    world.update(world.style.flare.pressDelay);
    CHECK(pressed == 1);
    world.update(world.style.flare.pressDelay);
    CHECK(pressed == 1);
}

TEST_CASE("A flare button traces its line slower than it goes out", "[engine][tween]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t play = ui.button("PLAY", [] {}, {AnchorPoint::TopLeft, 20.0f, 20.0f});
    ui.button("QUIT", [] {}, {AnchorPoint::TopLeft, 20.0f, 100.0f});
    world.update();
    const auto highlightOf = [&] { return world.ecs.getComponent<Panel>(play).highlight; };

    // The keyboard lights the focused button
    world.press({Key::Down});
    world.press({Key::Up});
    world.update(world.style.flare.fadeDuration);
    CHECK(highlightOf() < 1.0f);
    world.update(world.style.flare.traceDuration);
    CHECK(std::abs(highlightOf() - 1.0f) < 1e-6f);

    world.press({Key::Down});
    world.update(world.style.flare.fadeDuration);
    CHECK(std::abs(highlightOf()) < 1e-6f);
}

TEST_CASE("A pressed flare button shines brighter for a moment, then stays lit", "[engine][tween]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t play = ui.button("PLAY", [] {}, {AnchorPoint::TopLeft, 20.0f, 20.0f});
    world.update();
    const auto highlightOf = [&] { return world.ecs.getComponent<Panel>(play).highlight; };

    world.press({Key::Enter});
    CHECK(highlightOf() > 1.0f);

    world.update(world.style.flare.pressDuration);
    world.update(world.style.flare.fadeDuration);
    CHECK(std::abs(highlightOf() - 1.0f) < 1e-6f);
}

TEST_CASE("A header puts the title after the back button", "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int pressed = 0;
    const entity_t back =
        ui.header("Settings", [&pressed] { ++pressed; }, {AnchorPoint::TopLeft, 20.0f, 20.0f});

    world.update();
    REQUIRE(world.isFocused(back));
    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(pressed == 1);
    CHECK(world.textOf(back) == "<");
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

    world.clickAt(960.0f, 676.8f);
    world.update(world.style.flare.pressDelay);
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
    bool music = true;
    const entity_t play = menu.toggle("Music", [&] { return music; }, [&](bool value) { music = value; });
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

TEST_CASE("A button lights up while the pointer is over it, without growing", "[engine][tween]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    world.update();
    const auto highlightOf = [&](entity_t entity) { return world.ecs.getComponent<Panel>(entity).highlight; };

    // Focused, but the player uses the mouse and the pointer is elsewhere
    world.update(world.style.flare.traceDuration);
    CHECK(highlightOf(play) == 0.0f);

    world.input.mouse = {960.0f, 604.8f};
    world.update();
    world.update(0.05f);
    CHECK(highlightOf(play) > 0.0f);
    CHECK(highlightOf(play) < 1.0f);
    world.update(world.style.flare.traceDuration);
    CHECK(std::abs(highlightOf(play) - 1.0f) < 1e-6f);
    CHECK(world.ecs.getComponent<Scale>(play).value == 1.0f);

    world.input.mouse = {10.0f, 10.0f};
    world.update();
    world.update(world.style.flare.traceDuration);
    CHECK(std::abs(highlightOf(play)) < 1e-6f);
}

TEST_CASE("With the keyboard, the focused button lights up", "[engine][tween]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    const entity_t quit = menu.button("Quit", [] {});
    world.update();
    const auto highlightOf = [&](entity_t entity) { return world.ecs.getComponent<Panel>(entity).highlight; };

    world.press({Key::Down});
    world.update(world.style.flare.traceDuration);
    CHECK(std::abs(highlightOf(play)) < 1e-6f);
    CHECK(std::abs(highlightOf(quit) - 1.0f) < 1e-6f);
}

TEST_CASE("A button sounds when it lights up and when it is pressed, not when its menu appears",
          "[engine][interface][sound]")
{
    World world;
    const rtype::audio::SoundId hover{1};
    const rtype::audio::SoundId click{2};
    world.style.button.hoverSound = SoundEffect{hover, {.volume = 0.4f}};
    world.style.button.clickSound = SoundEffect{click, {}};
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    const entity_t quit = menu.button("Quit", [] {});
    // Nobody plays the sounds: they stay on the buttons
    const auto soundOf = [&](entity_t entity) -> std::optional<rtype::audio::SoundId> {
        if (!world.ecs.hasComponent<SoundEffect>(entity)) {
            return std::nullopt;
        }
        return world.ecs.getComponent<SoundEffect>(entity).sound;
    };
    world.update();
    CHECK_FALSE(soundOf(play));

    world.press({Key::Down});
    CHECK_FALSE(soundOf(play));
    REQUIRE(soundOf(quit) == hover);
    CHECK(world.ecs.getComponent<SoundEffect>(quit).settings.volume == 0.4f);

    world.press({Key::Enter});
    CHECK(soundOf(quit) == click);

    // With the keyboard, the first button of a menu that appears lights up silently
    MenuBuilder submenu(world.ecs, world.scenes, world.metrics, world.style, "Submenu");
    const entity_t back = submenu.button("Back", [] {});
    world.update();
    REQUIRE(world.isFocused(back));
    CHECK_FALSE(soundOf(back));

    // The pointer away, the panel goes out; over the button, it lights up again
    world.input.mouse = {10.0f, 10.0f};
    world.update();
    world.update(world.style.flare.traceDuration);
    world.ecs.removeComponent<SoundEffect>(back);
    const rtype::ecs::Position centre = world.ecs.getComponent<rtype::ecs::Position>(back);
    world.input.mouse = {centre.x, centre.y};
    world.update();
    CHECK(soundOf(back) == hover);
}

TEST_CASE("A slider and a switch sound as the buttons when they light up and when they are clicked",
          "[engine][interface][sound]")
{
    World world;
    const rtype::audio::SoundId hover{1};
    const rtype::audio::SoundId click{2};
    world.style.button.hoverSound = SoundEffect{hover, {}};
    world.style.button.clickSound = SoundEffect{click, {}};
    float volume = 0.5f;
    bool vibration = false;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t slider = ui.slider(
        "Volume", [&] { return volume; }, [&](float value) { volume = value; }, 0.1f,
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    const entity_t toggle = ui.toggle(
        "Vibration", [&] { return vibration; }, [&](bool value) { vibration = value; },
        {AnchorPoint::TopLeft, 100.0f, 300.0f});
    const auto soundOf = [&](entity_t entity) -> std::optional<rtype::audio::SoundId> {
        if (!world.ecs.hasComponent<SoundEffect>(entity)) {
            return std::nullopt;
        }
        return world.ecs.getComponent<SoundEffect>(entity).sound;
    };
    const auto pointAt = [&](entity_t entity) {
        const rtype::ecs::Position centre = world.ecs.getComponent<rtype::ecs::Position>(entity);
        world.input.mouse = {centre.x, centre.y};
        world.update();
        return centre;
    };
    world.update();

    pointAt(slider);
    CHECK(soundOf(slider) == hover);
    const rtype::ecs::Position row = pointAt(toggle);
    CHECK(soundOf(toggle) == hover);

    world.ecs.removeComponent<SoundEffect>(toggle);
    world.clickAt(row.x, row.y);
    CHECK(vibration);
    CHECK(soundOf(toggle) == click);

    // Set to the value it already has, the switch stays silent
    world.ecs.removeComponent<SoundEffect>(toggle);
    world.press({Key::Right});
    CHECK_FALSE(soundOf(toggle));

    const rtype::ecs::Position rail = pointAt(slider);
    world.ecs.removeComponent<SoundEffect>(slider);
    world.clickAt(rail.x, rail.y);
    CHECK(soundOf(slider) == click);
}

TEST_CASE("Without sounds in the style, the buttons are silent", "[engine][interface][sound]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    menu.button("Play", [] {});
    const entity_t quit = menu.button("Quit", [] {});
    world.update();

    world.press({Key::Down});
    world.press({Key::Enter});
    CHECK_FALSE(world.ecs.hasComponent<SoundEffect>(quit));
}

TEST_CASE("A button is as wide as its text and as high as the style says", "[engine][interface]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Menu");
    const entity_t play = menu.button("Play", [] {});
    world.update();

    const Hitbox &box = world.ecs.getComponent<Hitbox>(play);
    CHECK(box.baseWidth > 0.0f);
    CHECK(box.height == world.style.flare.height);
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

TEST_CASE("A named scene is open from the update that opens it to the update that closes it",
          "[engine][scenes]")
{
    Engine engine;
    engine.createScene("game");
    engine.createScene("console", {.overlay = true});
    engine.switchScene("game");
    engine.pushScene("console");
    CHECK_FALSE(engine.isSceneOpen("console"));

    engine.update(0.1f);
    CHECK(engine.isSceneOpen("game"));
    CHECK(engine.isSceneOpen("console"));

    engine.popScene();
    CHECK(engine.isSceneOpen("console"));
    engine.update(0.1f);
    CHECK_FALSE(engine.isSceneOpen("console"));
    CHECK(engine.isSceneOpen("game"));
    CHECK_FALSE(engine.isSceneOpen("menu"));
}

TEST_CASE("A text field shows its caption as its type says", "[engine][interface]")
{
    TextField field{.caption = ""};
    field.input.setText("help");
    CHECK(fieldPrefix(field).empty());
    CHECK(widgetLabel(field) == "help");

    field.caption = "Name";
    CHECK(widgetLabel(field) == "Name: help");

    field.caption = ">";
    field.type = TextFieldType::Console;
    CHECK(fieldPrefix(field) == "> ");
    CHECK(widgetLabel(field) == "> help");
}

TEST_CASE("A console text field keeps its size when it gets the focus", "[engine][tween]")
{
    World world;
    MenuBuilder menu(world.ecs, world.scenes, world.metrics, world.style, "Console");
    const entity_t field = menu.textField(">", "", {});
    world.ecs.getComponent<TextField>(field).type = TextFieldType::Console;
    world.update();

    for (int frame = 0; frame < 30; ++frame) {
        world.update(0.016f);
        CHECK_FALSE(world.ecs.hasComponent<Tween>(field));
    }
    REQUIRE(world.isFocused(field));
    REQUIRE(world.ecs.getComponent<Scale>(field).value == 1.0f);

    world.input.typed = U"help";
    world.update();
    REQUIRE(world.textOf(field) == "> help");
}

namespace
{

// The toasts of the default style are in the top right corner; with the fake measures, a short text fits in
// their minimum width.
[[nodiscard]] float opacityOf(World &world, entity_t toast)
{
    return world.ecs.getComponent<Opacity>(toast).value;
}

// Centre of the toast at a place of the stack, 0 being the nearest to the corner, once it is fully shown
[[nodiscard]] rtype::render::Vector2 toastCentreAt(const ToastStyle &style, float place, float width)
{
    const float step = style.height() + style.spacing;
    return {1920.0f - style.margin - (width / 2.0f), style.margin + (style.height() / 2.0f) + (place * step)};
}

[[nodiscard]] float widthOf(World &world, entity_t toast)
{
    return world.ecs.getComponent<Toast>(toast).width;
}

[[nodiscard]] std::size_t countToasts(World &world)
{
    const auto &toasts = world.ecs.cache<Toast>();
    std::size_t count = 0;
    for (entity_t entity = 0; entity < toasts.size(); ++entity) {
        count += toasts.has(entity) ? 1 : 0;
    }
    return count;
}

[[nodiscard]] bool isNear(float value, float expected)
{
    return std::abs(value - expected) < 0.01f;
}

} // namespace

TEST_CASE("Showing a toast creates it with its text and type", "[engine][toast]")
{
    World world;
    const entity_t toast = world.engine.showToast("Connection lost", ToastType::Error);
    const entity_t info = world.engine.showToast("Saved");

    REQUIRE(world.ecs.hasComponent<Toast>(toast));
    CHECK(world.ecs.getComponent<Toast>(toast).text == "Connection lost");
    CHECK(world.ecs.getComponent<Toast>(toast).type == ToastType::Error);
    CHECK(world.ecs.getComponent<Toast>(info).type == ToastType::Info);
}

TEST_CASE("A toast fades in, stays, fades out, then disappears", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t toast = world.engine.showToast("Saved");

    world.update(0.0f);
    CHECK(isNear(opacityOf(world, toast), 0.0f));
    world.update(style.fadeDuration / 2.0f);
    CHECK(isNear(opacityOf(world, toast), 0.5f));
    world.update(style.fadeDuration / 2.0f);
    CHECK(isNear(opacityOf(world, toast), 1.0f));
    world.update(style.duration);
    CHECK(isNear(opacityOf(world, toast), 1.0f));
    world.update(style.fadeDuration / 2.0f);
    CHECK(isNear(opacityOf(world, toast), 0.5f));
    world.update(style.fadeDuration / 2.0f);
    CHECK_FALSE(world.ecs.hasComponent<Toast>(toast));
}

TEST_CASE("A toast stays over every scene, and after a scene switch", "[engine][toast]")
{
    World world;
    world.engine.createScene("menu");
    world.engine.createScene("game");
    world.engine.switchScene("menu");
    world.update();
    const entity_t toast = world.engine.showToast("Saved");

    world.engine.switchScene("game");
    world.update(0.1f);

    REQUIRE(world.ecs.hasComponent<Toast>(toast));
    CHECK(world.scenes.isVisible(toast));
    CHECK(world.ecs.getComponent<Drawable>(toast).layer == Layer::Interface);
}

TEST_CASE("Toasts stack from their corner, the newest nearest to it", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t older = world.engine.showToast("First");
    const entity_t newer = world.engine.showToast("Second");

    world.update(1.0f);

    const rtype::render::Vector2 nearest = toastCentreAt(style, 0.0f, widthOf(world, newer));
    const rtype::render::Vector2 below = toastCentreAt(style, 1.0f, widthOf(world, older));
    CHECK(isNear(positionOf(world, newer).x, nearest.x));
    CHECK(isNear(positionOf(world, newer).y, nearest.y));
    CHECK(isNear(positionOf(world, older).x, below.x));
    CHECK(isNear(positionOf(world, older).y, below.y));
}

TEST_CASE("A new toast takes its place at once, the others glide to theirs", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t older = world.engine.showToast("First");
    world.update(1.0f);

    world.engine.showToast("Second");
    world.update(0.01f);
    const float olderY = positionOf(world, older).y;
    CHECK(olderY > toastCentreAt(style, 0.0f, widthOf(world, older)).y);
    CHECK(olderY < toastCentreAt(style, 1.0f, widthOf(world, older)).y);

    world.update(1.0f);
    CHECK(isNear(positionOf(world, older).y, toastCentreAt(style, 1.0f, widthOf(world, older)).y));
}

TEST_CASE("Toasts in a bottom corner stack upwards", "[engine][toast]")
{
    World world;
    ToastStyle &style = world.style.toast;
    style.corner = AnchorPoint::BottomLeft;
    const entity_t older = world.engine.showToast("First");
    const entity_t newer = world.engine.showToast("Second");

    world.update(1.0f);

    const float nearestY = 1080.0f - style.margin - (style.height() / 2.0f);
    CHECK(isNear(positionOf(world, newer).x, style.margin + (widthOf(world, newer) / 2.0f)));
    CHECK(isNear(positionOf(world, newer).y, nearestY));
    CHECK(isNear(positionOf(world, older).y, nearestY - style.height() - style.spacing));
}

TEST_CASE("A toast is wide enough for its text", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const std::string text(80, 'x');
    const entity_t toast = world.engine.showToast(text);

    world.update(1.0f);

    // The message, then the icon, after a gap
    const float textWidth = (0.5f * style.nameSize * static_cast<float>(text.size())) +
                            (style.letterSpacing * static_cast<float>(text.size() - 1));
    const float width = textWidth + style.gap + style.iconSize;
    CHECK(isNear(world.ecs.getComponent<Toast>(toast).width, width));
    CHECK(isNear(positionOf(world, toast).x, toastCentreAt(style, 0.0f, width).x));
}

TEST_CASE("A toast reads as a line: its subject, its message, its actor, then its icon", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t toast = world.engine.showToast("ORBITAL MODULE", "taken by", "KAZE", ToastType::Success);
    const entity_t alone = world.engine.showToast("Settings saved");

    world.update(1.0f);

    const Toast &line = world.ecs.getComponent<Toast>(toast);
    CHECK(line.subject == "ORBITAL MODULE");
    CHECK(line.text == "taken by");
    CHECK(line.actor == "KAZE");
    CHECK(line.type == ToastType::Success);
    const auto names = [&](std::size_t letters) {
        return (0.5f * style.nameSize * static_cast<float>(letters)) +
               (style.letterSpacing * static_cast<float>(letters - 1));
    };
    const float message = 0.5f * style.messageSize * 8.0f;
    CHECK(isNear(line.width,
                 names(14) + style.gap + message + style.gap + names(4) + style.gap + style.iconSize));

    // A message alone is shown as the subject
    const Toast &single = world.ecs.getComponent<Toast>(alone);
    CHECK(single.getHeadline() == "Settings saved");
    CHECK(single.getBody().empty());
    CHECK(isNear(single.width, names(14) + style.gap + style.iconSize));
}

TEST_CASE("Beyond the maximum, the oldest toast fades out", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t oldest = world.engine.showToast("0");
    world.update(style.fadeDuration);
    for (std::size_t count = 1; count < style.maxVisible; ++count) {
        world.engine.showToast(std::to_string(count));
        world.update(style.fadeDuration);
    }
    CHECK(isNear(opacityOf(world, oldest), 1.0f));

    world.engine.showToast("One too many");
    world.update(style.fadeDuration / 2.0f);
    CHECK(isNear(opacityOf(world, oldest), 0.5f));
    world.update(style.fadeDuration / 2.0f);
    CHECK_FALSE(world.ecs.hasComponent<Toast>(oldest));
    CHECK(countToasts(world) == style.maxVisible);
}

TEST_CASE("The handle of a slider lights up only while the pointer is over it", "[engine][interface][slider]")
{
    World world;
    float volume = 0.5f;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t slider = ui.slider(
        "Volume", [&] { return volume; }, [&](float value) { volume = value; }, 0.1f,
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();
    const SliderStyle &style = world.style.slider;
    const rtype::ecs::Position rail = world.ecs.getComponent<rtype::ecs::Position>(slider);
    const SliderTrack &track = world.ecs.getComponent<SliderTrack>(slider);
    const float handleX = style.getRailLeft(rail.x) + (0.5f * style.getRailWidth());
    const float railY = style.getRailY(rail.y);

    // On the slider, but far from the handle: the caption lights up, the handle does not
    world.input.mouse = {handleX - 200.0f, railY};
    world.update();
    world.update(style.fadeDuration);
    CHECK(track.hover < 0.01f);
    CHECK(track.focus > 0.99f);

    // Over the handle
    world.input.mouse = {handleX + 4.0f, railY + 3.0f};
    world.update();
    world.update(style.fadeDuration);
    CHECK(track.hover > 0.99f);

    // Away again
    world.input.mouse = {handleX, railY + 200.0f};
    world.update();
    world.update(style.fadeDuration);
    CHECK(track.hover < 0.01f);
}

TEST_CASE("A switch shows its toggle, slides its dot, and lights up under the pointer",
          "[engine][interface][switch]")
{
    World world;
    bool vibration = false;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t entity = ui.toggle(
        "Vibration", [&] { return vibration; }, [&](bool value) { vibration = value; },
        {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();
    const SwitchStyle &style = world.style.toggle;
    const SwitchTrack &track = world.ecs.getComponent<SwitchTrack>(entity);
    const rtype::ecs::Position row = world.ecs.getComponent<rtype::ecs::Position>(entity);

    // Its row is its hitbox, and Confirm sets it
    const Hitbox &box = world.ecs.getComponent<Hitbox>(entity);
    CHECK(box.width == style.width);
    CHECK(box.height == style.height);
    world.press({Key::Enter});
    CHECK(vibration);

    // The dot slides to the ON side, not at once
    world.update(style.slideDuration / 2.0f);
    CHECK(track.position > 0.01f);
    CHECK(track.position < 0.99f);
    world.update(style.slideDuration);
    CHECK(track.position > 0.99f);

    // The part of the row on the right lights up; the caption on the left only lights the caption
    world.input.mouse = {row.x - (style.width / 2.0f) + 40.0f, row.y};
    world.update();
    world.update(style.slideDuration);
    CHECK(track.hover < 0.01f);
    CHECK(track.focus > 0.99f);
    world.input.mouse = {row.x + (style.width / 2.0f) - 20.0f, row.y};
    world.update();
    world.update(style.slideDuration);
    CHECK(track.hover > 0.99f);

    // A click on the row sets the toggle back
    world.clickAt(row.x - (style.width / 2.0f) + 40.0f, row.y);
    CHECK_FALSE(vibration);
}

TEST_CASE("A switch lights up only over what is drawn of it, not over the rest of its row",
          "[engine][interface][switch]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t entity =
        ui.toggle("Vibration", [] { return false; }, [](bool) {}, {AnchorPoint::TopLeft, 100.0f, 100.0f});
    world.update();
    const SwitchStyle &style = world.style.toggle;
    const SwitchTrack &track = world.ecs.getComponent<SwitchTrack>(entity);
    const rtype::ecs::Position row = world.ecs.getComponent<rtype::ecs::Position>(entity);
    const auto measure = [&](const char *label, float size, float spacing) {
        const std::string text = label;
        return rtype::render::Vector2{(0.5f * size * static_cast<float>(text.size())) +
                                          (spacing * static_cast<float>(text.size() - 1)),
                                      size};
    };
    const SwitchLayout layout = computeSwitchLayout(style, {row.x, row.y}, measure);
    const auto hoveredAt = [&](float x, float y) {
        world.input.mouse = {x, y};
        world.update();
        world.update(style.slideDuration);
        return track.hover > 0.99f;
    };

    // Inside: on the "OFF" label, on the link, on the "ON" label, and in the small margin around them
    CHECK(hoveredAt(layout.offX + 2.0f, row.y));
    CHECK(hoveredAt((layout.linkLeft + layout.linkRight) / 2.0f, row.y));
    CHECK(hoveredAt(layout.onX + 2.0f, row.y));
    CHECK(hoveredAt(layout.offX - (style.hoverMargin / 2.0f), row.y));
    // Outside: just beyond the margin on the left and on the right, and above and below the labels, although
    // still inside the row
    CHECK_FALSE(hoveredAt(layout.offX - style.hoverMargin - 2.0f, row.y));
    CHECK_FALSE(hoveredAt(layout.hoverArea.x + layout.hoverArea.width + 2.0f, row.y));
    CHECK_FALSE(hoveredAt(layout.linkLeft + 10.0f, layout.hoverArea.y - 2.0f));
    CHECK_FALSE(hoveredAt(layout.linkLeft + 10.0f, layout.hoverArea.y + layout.hoverArea.height + 2.0f));
    // The caption on the left lights the caption only
    CHECK_FALSE(hoveredAt(row.x - (style.width / 2.0f) + 20.0f, row.y));
    CHECK(track.focus > 0.99f);
    // The area is as high as what is drawn, not as the whole row
    CHECK(layout.hoverArea.height < style.height);
}

TEST_CASE("A gauge shows the value it reads, takes no focus, and is placed by its size",
          "[engine][interface][gauge]")
{
    World world;
    float charge = 0.25f;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t gauge = ui.gauge("Charge", [&] { return charge; }, {AnchorPoint::TopLeft, 100.0f, 200.0f});
    const entity_t button = ui.button("Play", [] {}, {AnchorPoint::TopLeft, 100.0f, 400.0f});
    world.update();

    const GaugeStyle &style = world.style.gauge;
    CHECK_FALSE(world.ecs.hasComponent<Focusable>(gauge));
    CHECK(world.isFocused(button));
    // Its top left corner is at its anchor
    const rtype::ecs::Position centre = world.ecs.getComponent<rtype::ecs::Position>(gauge);
    CHECK(isNear(centre.x - (style.width / 2.0f), 100.0f));
    CHECK(isNear(centre.y - (style.getHeight() / 2.0f), 200.0f));
    CHECK(world.ecs.getComponent<Gauge>(gauge).get() == 0.25f);
    charge = 1.0f;
    CHECK(world.ecs.getComponent<Gauge>(gauge).get() == 1.0f);
}

TEST_CASE("Announcements and banners are toasts of their own kind, with a stack each", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t line = world.engine.showToast("Saved");
    const entity_t announcement = world.engine.showAnnouncement("PLAYER 2 JOINED", "Room", ToastType::Info);
    const entity_t banner = world.engine.showBanner("SECTOR 02", "DEBRIS FIELD", ToastType::Error);

    world.update(1.0f);

    CHECK(world.ecs.getComponent<Toast>(line).kind == ToastKind::Line);
    const Toast &announced = world.ecs.getComponent<Toast>(announcement);
    CHECK(announced.kind == ToastKind::Announcement);
    CHECK(announced.getHeadline() == "PLAYER 2 JOINED");
    CHECK(announced.getBody() == "Room");
    CHECK(world.ecs.getComponent<Toast>(banner).kind == ToastKind::Banner);

    // Each kind starts its own stack: all three are at their first place
    CHECK(isNear(world.ecs.getComponent<Toast>(line).place, 0.0f));
    CHECK(isNear(announced.place, 0.0f));
    CHECK(isNear(world.ecs.getComponent<Toast>(banner).place, 0.0f));

    // An announcement is centred at the top of the screen, a banner across the whole screen
    CHECK(isNear(positionOf(world, announcement).x, 960.0f));
    CHECK(isNear(positionOf(world, announcement).y,
                 style.announcementTop + (style.getHeight(ToastKind::Announcement) / 2.0f)));
    CHECK(isNear(world.ecs.getComponent<Toast>(banner).width, 1920.0f));
    CHECK(isNear(positionOf(world, banner).x, 960.0f));
    CHECK(isNear(positionOf(world, banner).y, style.bannerCentre * 1080.0f));
}

TEST_CASE("Beyond their maximum, the oldest announcement and banner leave", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const auto count = [&](ToastKind kind) {
        const auto &toasts = world.ecs.cache<Toast>();
        std::size_t total = 0;
        for (entity_t entity = 0; entity < toasts.size(); ++entity) {
            total += (toasts.has(entity) && toasts.get(entity).kind == kind) ? 1 : 0;
        }
        return total;
    };
    for (std::size_t shown = 0; shown <= style.maxAnnouncements; ++shown) {
        world.engine.showAnnouncement("TITLE", "Detail");
        world.update(style.announcementIn);
    }
    // The oldest one is leaving: it is gone once its exit is over
    world.update(style.announcementOut);
    CHECK(count(ToastKind::Announcement) == style.maxAnnouncements);

    world.engine.showBanner("FIRST", "Banner");
    world.update(style.bannerIn);
    world.engine.showBanner("SECOND", "Banner");
    world.update(style.bannerOut / 2.0f);
    CHECK(count(ToastKind::Banner) == 2);
    world.update(style.bannerOut);
    CHECK(count(ToastKind::Banner) == 1);
}

TEST_CASE("Announcements and banners appear and leave over their own time", "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const entity_t announcement = world.engine.showAnnouncement("TITLE", "Detail");
    const entity_t banner = world.engine.showBanner("TITLE", "Detail");

    world.update(0.0f);
    CHECK(isNear(opacityOf(world, announcement), 0.0f));
    CHECK(isNear(opacityOf(world, banner), 0.0f));

    // Half way through their entrance: the opacity is the progress that the drawing follows
    world.update(style.announcementIn / 2.0f);
    CHECK(isNear(opacityOf(world, announcement), 0.5f));
    CHECK(opacityOf(world, banner) < 0.5f);
    world.update(style.bannerIn);
    CHECK(isNear(opacityOf(world, announcement), 1.0f));
    CHECK(isNear(opacityOf(world, banner), 1.0f));

    // They stay for the duration of the style, then leave over their own time
    const float elapsed = (style.announcementIn / 2.0f) + style.bannerIn;
    world.update(style.getLifetime(ToastKind::Announcement) - (style.announcementOut / 2.0f) - elapsed);
    CHECK(isNear(opacityOf(world, announcement), 0.5f));

    world.update(style.announcementOut / 2.0f);
    CHECK_FALSE(world.ecs.hasComponent<Toast>(announcement));
    CHECK(world.ecs.hasComponent<Toast>(banner));
}

TEST_CASE("A banner given a duration stays fully shown for it, instead of the duration of the style",
          "[engine][toast]")
{
    World world;
    const ToastStyle &style = world.style.toast;
    const float duration = style.duration / 2.0f;
    const entity_t shorter = world.engine.showBanner("WAVE 03", "Detail", ToastType::Info, duration);

    world.update(0.0f);
    CHECK(world.ecs.getComponent<Toast>(shorter).duration == std::optional<float>{duration});

    // Half way through its exit, after its own duration
    world.update(style.bannerIn + duration + (style.bannerOut / 2.0f));
    CHECK(isNear(opacityOf(world, shorter), 0.5f));
    world.update(style.bannerOut / 2.0f);
    CHECK_FALSE(world.ecs.hasComponent<Toast>(shorter));
}

TEST_CASE("A spinning model turns at its speed and stays between 0 and 360 degrees", "[engine][spin]")
{
    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<SpinSystem>();
    const entity_t ring = ecs.createEntity();
    ecs.addComponent<Model>(ring);
    ecs.addComponent<Spin>(ring, 90.0f);
    const entity_t backwards = ecs.createEntity();
    ecs.addComponent<Model>(backwards);
    ecs.addComponent<Spin>(backwards, -90.0f);
    const entity_t still = ecs.createEntity();
    ecs.addComponent<Model>(still);

    engine.update(1.0f);
    CHECK(ecs.getComponent<Model>(ring).spinAngle == 90.0f);
    CHECK(ecs.getComponent<Model>(backwards).spinAngle == 270.0f);
    CHECK(ecs.getComponent<Model>(still).spinAngle == 0.0f);

    engine.update(3.5f);
    CHECK(ecs.getComponent<Model>(ring).spinAngle == 45.0f);
    CHECK(ecs.getComponent<Model>(backwards).spinAngle == 315.0f);
}

TEST_CASE("A scrolling picture moves its offset by its speed", "[engine][scroll]")
{
    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<ScrollSystem>();
    const entity_t far = ecs.createEntity();
    ecs.addComponent<Scroll>(far, 10.0f);
    const entity_t backwards = ecs.createEntity();
    ecs.addComponent<Scroll>(backwards, -40.0f);

    engine.update(0.5f);
    CHECK(ecs.getComponent<Scroll>(far).offset == 5.0f);
    CHECK(ecs.getComponent<Scroll>(backwards).offset == -20.0f);

    engine.update(1.5f);
    CHECK(ecs.getComponent<Scroll>(far).offset == 20.0f);
}

TEST_CASE("A model animation moves its time by the elapsed time", "[engine][animation]")
{
    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<ModelAnimationSystem>();
    const entity_t ship = ecs.createEntity();
    ecs.addComponent<ModelAnimation>(ship, 2);
    const entity_t restarted = ecs.createEntity();
    ecs.addComponent<ModelAnimation>(restarted, 0, 1.5f);

    engine.update(0.25f);
    CHECK(ecs.getComponent<ModelAnimation>(ship).animation == 2);
    CHECK(ecs.getComponent<ModelAnimation>(ship).time == 0.25f);
    CHECK(ecs.getComponent<ModelAnimation>(restarted).time == 1.75f);

    ecs.getComponent<ModelAnimation>(ship) = {1, 0.0f};
    engine.update(0.5f);
    CHECK(ecs.getComponent<ModelAnimation>(ship).animation == 1);
    CHECK(ecs.getComponent<ModelAnimation>(ship).time == 0.5f);

    // Held: its time stays
    ecs.getComponent<ModelAnimation>(ship).speed = 0.0f;
    engine.update(0.5f);
    CHECK(ecs.getComponent<ModelAnimation>(ship).time == 0.5f);
}

TEST_CASE("Switching a model animation fades from the previous one", "[engine][animation]")
{
    ModelAnimation animation{2, 1.5f};
    switchModelAnimation(animation, 3, 0.75f);
    CHECK(animation.animation == 3);
    CHECK(animation.time == 0.0f);
    CHECK(animation.previousAnimation == 2);
    CHECK(animation.previousTime == 1.5f);
    CHECK(animation.fadeDuration == 0.75f);
}

TEST_CASE("An animation added over a model animation plays once, then is removed", "[engine][animation]")
{
    Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<ModelAnimationSystem>();
    const entity_t ship = ecs.createEntity();
    ModelAnimation &animation = ecs.addComponent<ModelAnimation>(ship, 2);
    CHECK(animation.addedAnimation == NoAnimation);

    addModelAnimation(animation, 4, 0.3f);
    engine.update(0.25f);
    const ModelAnimation &played = ecs.getComponent<ModelAnimation>(ship);
    CHECK(played.animation == 2);
    CHECK(played.time == 0.25f);
    CHECK(played.addedAnimation == 4);
    CHECK(played.addedTime == 0.25f);

    // Added again: from its start
    addModelAnimation(ecs.getComponent<ModelAnimation>(ship), 4, 0.3f);
    engine.update(0.25f);
    CHECK(ecs.getComponent<ModelAnimation>(ship).addedTime == 0.25f);

    // Played for its duration: removed, the pose is the one of the other entities of the model again
    engine.update(0.25f);
    CHECK(ecs.getComponent<ModelAnimation>(ship).addedAnimation == NoAnimation);
    CHECK(ecs.getComponent<ModelAnimation>(ship) == ModelAnimation{2, 0.75f});
}

TEST_CASE("The sound system plays each sound effect once, then takes it from its entity", "[engine][sound]")
{
    // Audio is disabled: the sound file is only checked to exist, and nothing is heard
    const testing::TempDirectory assets("rtype_engine_sounds");
    std::ofstream(assets.path / "shot.wav") << "fake";
    {
        const rtype::audio::AudioDevice device({.enabled = false});
        Engine engine;
        rtype::audio::AudioPlayer audio(device, assets.path, engine.getThreads().getAssetsWorker());
        rtype::ecs::Ecs &ecs = engine.getEcs();
        ecs.registerSystem<SoundSystem>(audio);
        const entity_t shot = ecs.createEntity();
        ecs.addComponent<SoundEffect>(shot, audio.loadSound("shot.wav"),
                                      rtype::audio::PlaySettings{.pan = -0.5f});

        engine.update(0.016f);
        CHECK(ecs.isAlive(shot));
        CHECK_FALSE(ecs.hasComponent<SoundEffect>(shot));
        engine.update(0.016f);
        CHECK_FALSE(ecs.hasComponent<SoundEffect>(shot));
    }
}

namespace
{

// Completed at its third wake
class ThirdWakeQuest : public QuestSystem
{
    bool progress() override { return ++wakes == 3; }

public:
    int wakes = 0;

    ThirdWakeQuest(rtype::ecs::ComponentCaches & /*caches*/, Settings &settings)
        : QuestSystem("third_wake", {"hit", "miss"}, settings)
    {
    }
};

} // namespace

TEST_CASE("A quest sleeps until one of its events is triggered", "[engine][quest]")
{
    Engine engine;
    auto &quest = engine.getEcs().registerSystem<ThirdWakeQuest>(engine.getSettings());
    engine.addQuest(quest);
    CHECK(engine.hasEvent("hit"));
    CHECK(engine.hasEvent("miss"));

    engine.update(0.1f);
    CHECK(quest.wakes == 0);

    engine.triggerEvent("hit");
    engine.update(0.1f);
    CHECK(quest.wakes == 1);
    CHECK_FALSE(quest.enabled);
    engine.update(0.1f);
    CHECK(quest.wakes == 1);

    engine.triggerEvent("miss");
    engine.update(0.1f);
    CHECK(quest.wakes == 2);
}

TEST_CASE("Triggers between two updates wake a quest once", "[engine][quest]")
{
    Engine engine;
    auto &quest = engine.getEcs().registerSystem<ThirdWakeQuest>(engine.getSettings());
    engine.addQuest(quest);

    engine.triggerEvent("hit");
    engine.triggerEvent("hit");
    engine.triggerEvent("miss");
    engine.update(0.1f);
    CHECK(quest.wakes == 1);
}

TEST_CASE("A completed quest is written in the settings and told once", "[engine][quest]")
{
    Engine engine;
    auto &quest = engine.getEcs().registerSystem<ThirdWakeQuest>(engine.getSettings());
    int completions = 0;
    quest.setCompletionHandler([&completions](const QuestSystem &completed) {
        CHECK(completed.getId() == "third_wake");
        ++completions;
    });
    engine.addQuest(quest);

    for (int wake = 0; wake < 5; ++wake) {
        engine.triggerEvent("hit");
        engine.update(0.1f);
    }
    CHECK(quest.wakes == 3);
    CHECK(completions == 1);
    CHECK(quest.isCompleted());
    CHECK(engine.getSettings().getBool("quest.third_wake") == true);
}

TEST_CASE("A quest completed at a previous run never wakes", "[engine][quest]")
{
    Engine engine;
    engine.getSettings().setBool("quest.third_wake", true);
    auto &quest = engine.getEcs().registerSystem<ThirdWakeQuest>(engine.getSettings());
    engine.addQuest(quest);
    CHECK(quest.isCompleted());
    CHECK_FALSE(engine.hasEvent("hit"));

    engine.createEvent("hit");
    engine.triggerEvent("hit");
    engine.update(0.1f);
    CHECK(quest.wakes == 0);
    CHECK_FALSE(quest.enabled);
}

TEST_CASE("A field of a form keeps its caption and its size, and lights up while it has the focus",
          "[engine][interface][field]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t name = ui.textField("PSEUDO", "Kaze", {AnchorPoint::TopLeft, 100.0f, 200.0f}, 16);
    const entity_t password = ui.passwordField("MOT DE PASSE", {AnchorPoint::TopLeft, 100.0f, 400.0f});
    world.update();

    const FieldStyle &style = world.style.field;
    const Hitbox &hitbox = world.ecs.getComponent<Hitbox>(name);
    CHECK(hitbox.baseWidth == style.width);
    CHECK(hitbox.baseHeight == style.getHeight());
    CHECK(isNear(positionOf(world, name).y - (style.getHeight() / 2.0f), 200.0f));
    REQUIRE(world.isFocused(name));

    world.input.typed = U"!";
    world.update(style.fadeDuration);
    CHECK(world.ecs.getComponent<TextField>(name).input.text() == "Kaze!");
    CHECK(world.textOf(name) == "PSEUDO");
    CHECK(world.ecs.getComponent<TextField>(name).light == 1.0f);
    CHECK_FALSE(world.ecs.hasComponent<Tween>(name));

    // Tab goes to the next field, which lights up while the first one goes out
    world.press({Key::Tab});
    REQUIRE(world.isFocused(password));
    world.update(style.fadeDuration);
    CHECK(world.ecs.getComponent<TextField>(name).light == 0.0f);
    CHECK(world.ecs.getComponent<TextField>(password).light == 1.0f);
    CHECK(world.ecs.getComponent<TextField>(password).input.isSecret());
}

TEST_CASE("A field of a form loses its error once its content changes", "[engine][interface][field]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t email = ui.textField("EMAIL", "kaze@", {AnchorPoint::TopLeft, 100.0f, 200.0f});
    world.update();
    world.ecs.getComponent<TextField>(email).error = "Invalid email";

    world.update();
    CHECK(world.ecs.getComponent<TextField>(email).error == "Invalid email");
    world.input.typed = U"x";
    world.update();
    CHECK(world.ecs.getComponent<TextField>(email).error.empty());
}

TEST_CASE("A code takes digits only, and is sent as soon as it is full", "[engine][interface][field]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    std::vector<std::string> sent;
    const entity_t code = ui.codeField("CODE", 6, [&](const std::string &text) { sent.push_back(text); },
                                       {AnchorPoint::TopLeft, 100.0f, 200.0f});
    world.update();

    const FieldStyle &style = world.style.field;
    CHECK(world.ecs.getComponent<Hitbox>(code).baseWidth == style.getCodeWidth(6));
    CHECK(world.ecs.getComponent<Hitbox>(code).baseHeight == style.getCodeHeight());

    world.input.typed = U"48a1";
    world.update();
    CHECK(world.ecs.getComponent<TextField>(code).input.text() == "481");
    CHECK(sent.empty());

    world.input.typed = U"2930";
    world.update();
    CHECK(world.ecs.getComponent<TextField>(code).input.text() == "481293");
    REQUIRE(sent == std::vector<std::string>{"481293"});
}

TEST_CASE("A row of a list is pressed as a button; without action it is dim and skipped",
          "[engine][interface][list]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    int joined = 0;
    const entity_t full =
        ui.listRow("DEBRIS 9", {.side = "FULL", .width = 560.0f}, {}, {AnchorPoint::TopLeft, 100.0f, 200.0f});
    const entity_t open = ui.listRow("NEBULA 4", {.detail = "3 / 4", .width = 560.0f},
                                     [&joined] { ++joined; }, {AnchorPoint::TopLeft, 100.0f, 300.0f});
    world.update();

    CHECK(world.ecs.getComponent<Hitbox>(full).baseWidth == 560.0f);
    CHECK(world.ecs.getComponent<Hitbox>(open).baseHeight == world.style.list.height);
    CHECK(isNear(positionOf(world, full).x - 280.0f, 100.0f));
    CHECK_FALSE(world.ecs.hasComponent<Focusable>(full));
    REQUIRE(world.isFocused(open));

    world.press({Key::Enter});
    world.update(world.style.flare.pressDelay);
    CHECK(joined == 1);
}

TEST_CASE("An avatar is placed by its size and takes no focus; a danger button is a red button",
          "[engine][interface]")
{
    World world;
    InterfaceBuilder ui(world.ecs, world.scenes.open(), world.style);
    const entity_t avatar = ui.avatar({.initial = "K", .kind = AvatarKind::Discord, .size = 120.0f},
                                      {AnchorPoint::TopRight, -40.0f, 40.0f});
    const entity_t remove = ui.dangerButton("DELETE", [] {}, {AnchorPoint::TopLeft, 100.0f, 400.0f});
    world.update();

    CHECK_FALSE(world.ecs.hasComponent<Focusable>(avatar));
    CHECK(isNear(positionOf(world, avatar).x, 1920.0f - 40.0f - 60.0f));
    CHECK(isNear(positionOf(world, avatar).y, 40.0f + 60.0f));
    CHECK(world.ecs.getComponent<Panel>(remove).danger);
    CHECK(world.isFocused(remove));
}
