// Tests of the keys of the game chosen on the Controls page: the names of the keys, their assignment to the
// actions, how they are kept in the settings, and the capture of the key the player presses.

#include "Controls/KeyCaptureSystem.hpp"
#include "FakeInput.hpp"
#include "Settings/ControlSettings.hpp"
#include "rtype/engine/Engine/Engine.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using rtype::engine::Control;
using rtype::engine::Controls;
using rtype::input::Binding;
using rtype::input::Key;
using namespace rtype::client;

TEST_CASE("Every key has a name that gives it back", "[settings][input]")
{
    for (const Key key : rtype::input::allKeys()) {
        CAPTURE(rtype::input::keyName(key));
        CHECK(rtype::input::keyFromName(rtype::input::keyName(key)) == key);
    }
    CHECK(rtype::input::keyName(Key::LeftShift) == "LeftShift");
    CHECK_FALSE(rtype::input::keyFromName("Shift"));
}

TEST_CASE("A binding tells its key, and only a key", "[settings][input]")
{
    CHECK(Binding::key(Key::Space).asKey() == Key::Space);
    CHECK_FALSE(Binding::gamepadButton(rtype::input::GamepadButton::FaceDown).asKey());
}

TEST_CASE("A binding tells its gamepad button or axis, and only those", "[settings][input]")
{
    using rtype::input::AxisDirection;
    using rtype::input::GamepadAxis;
    using rtype::input::GamepadButton;
    CHECK(Binding::gamepadButton(GamepadButton::LeftBumper).asGamepadButton() == GamepadButton::LeftBumper);
    CHECK_FALSE(Binding::gamepadButton(GamepadButton::LeftBumper).asGamepadAxis());
    CHECK(Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative).asGamepadAxis() ==
          GamepadAxis::LeftY);
    CHECK_FALSE(Binding::key(Key::Space).asGamepadButton());
    CHECK_FALSE(Binding::key(Key::Space).asGamepadAxis());
}

TEST_CASE("The Controls page shows each gamepad input of an action once", "[settings]")
{
    const Controls controls;

    // The D-pad, then both directions of the left stick under one name
    CHECK(gamepadNamesOf(controls, Control::Up) == std::vector<std::string>{"pad.dpad_up", "pad.left_stick"});
    CHECK(gamepadNamesOf(controls, Control::FaceDown) == std::vector<std::string>{"pad.a"});
    CHECK(gamepadNamesOf(controls, Control::LeftShoulder) == std::vector<std::string>{"pad.lb"});
}

TEST_CASE("The action that already has a key is found before it is taken", "[settings]")
{
    Controls controls;

    const GameAction *owner = findKeyOwner(controls, Control::FaceDown, Key::W);
    REQUIRE(owner != nullptr);
    CHECK(owner->control == Control::Up);
    // The action itself, and a key no action has, have no other owner
    CHECK(findKeyOwner(controls, Control::Up, Key::W) == nullptr);
    CHECK(findKeyOwner(controls, Control::FaceDown, Key::I) == nullptr);

    assignKey(controls, Control::FaceDown, Key::W);
    CHECK(findKeyOwner(controls, Control::Up, Key::W)->control == Control::FaceDown);
}

TEST_CASE("The keys of the voice are V, M and N, and LT, Y and View on a gamepad", "[settings][voice]")
{
    Controls controls;

    CHECK(keysOf(controls, PushToTalkControl) == std::vector{Key::V});
    CHECK(keysOf(controls, MuteControl) == std::vector{Key::M});
    CHECK(keysOf(controls, DeafenControl) == std::vector{Key::N});
    CHECK(gamepadNamesOf(controls, PushToTalkControl) == std::vector<std::string>{"pad.lt"});
    CHECK(gamepadNamesOf(controls, MuteControl) == std::vector<std::string>{"pad.y"});
    CHECK(gamepadNamesOf(controls, DeafenControl) == std::vector<std::string>{"pad.view"});
    // No action of the game reads them
    for (const GameAction &action : GameActions) {
        for (const GameAction &voice : VoiceActions) {
            CHECK(action.control != voice.control);
        }
    }
}

TEST_CASE("A key does one thing among the actions of the game and those of the voice", "[settings][voice]")
{
    Controls controls;

    // V talks: Fire asks before taking it, and takes it from push-to-talk
    REQUIRE(findKeyOwner(controls, Control::FaceDown, Key::V) != nullptr);
    CHECK(findKeyOwner(controls, Control::FaceDown, Key::V)->name == "push_to_talk");
    assignKey(controls, Control::FaceDown, Key::V);
    CHECK(keysOf(controls, PushToTalkControl).empty());
    // And the other way around
    assignKey(controls, MuteControl, Key::V);
    CHECK(keysOf(controls, Control::FaceDown).empty());
    CHECK(keysOf(controls, MuteControl) == std::vector{Key::V});

    rtype::engine::Settings settings;
    keepControlSettings(settings, controls);
    CHECK(settings.getString("controls.mute") == "V");
    CHECK(settings.getString("controls.push_to_talk") == "");
    Controls next;
    applyControlSettings(settings, next);
    CHECK(keysOf(next, MuteControl) == std::vector{Key::V});
    CHECK(keysOf(next, PushToTalkControl).empty());
}

TEST_CASE("A key chosen replaces the keys of the action, and its gamepad buttons stay", "[settings]")
{
    Controls controls;
    CHECK(keysOf(controls, Control::Up) == std::vector{Key::Up, Key::W});

    assignKey(controls, Control::Up, Key::I);

    CHECK(keysOf(controls, Control::Up) == std::vector{Key::I});
    CHECK(controls.getBindings(Control::Up).size() == 3); // I, the D-pad and the stick
}

TEST_CASE("A key chosen leaves the other action that had it", "[settings]")
{
    Controls controls;

    assignKey(controls, Control::FaceDown, Key::W);

    CHECK(keysOf(controls, Control::FaceDown) == std::vector{Key::W});
    CHECK(keysOf(controls, Control::Up) == std::vector{Key::Up});
}

TEST_CASE("The keys are kept in the settings and found again", "[settings]")
{
    Controls controls;
    assignKey(controls, Control::FaceDown, Key::J);
    assignKey(controls, Control::Up, Key::Space); // Space leaves Fire, which keeps J
    rtype::engine::Settings settings;
    keepControlSettings(settings, controls);

    CHECK(settings.getString("controls.fire") == "J");
    CHECK(settings.getString("controls.move_up") == "Space");
    CHECK(settings.getString("controls.move_down") == "Down,S");

    Controls next;
    applyControlSettings(settings, next);
    CHECK(keysOf(next, Control::FaceDown) == std::vector{Key::J});
    CHECK(keysOf(next, Control::Up) == std::vector{Key::Space});
    CHECK(keysOf(next, Control::Down) == std::vector{Key::Down, Key::S});
}

TEST_CASE("Wrong keys in the settings leave the default ones, an empty list leaves none", "[settings]")
{
    rtype::engine::Settings settings;
    settings.setString("controls.fire", "Space,Laser");
    settings.setString("controls.aim_up", "");
    Controls controls;

    applyControlSettings(settings, controls);

    CHECK(keysOf(controls, Control::FaceDown) == std::vector{Key::Space});
    CHECK(keysOf(controls, Control::LeftShoulder).empty());
}

TEST_CASE("Putting the default keys back undoes the keys chosen", "[settings]")
{
    Controls controls;
    assignKey(controls, Control::Up, Key::Space);

    controls.resetDefaults();

    CHECK(keysOf(controls, Control::Up) == std::vector{Key::Up, Key::W});
    CHECK(keysOf(controls, Control::FaceDown) == std::vector{Key::Space});
}

namespace
{

/// An engine with the capture system reading a fake keyboard, and an entity waiting for a key in an open
/// scene.
struct Capture {
    FakeInput input;
    rtype::engine::Engine engine;
    std::optional<Key> given;

    Capture()
    {
        rtype::ecs::Ecs &ecs = engine.getEcs();
        ecs.registerSystem<KeyCaptureSystem>(ecs, engine.getScenes(), input);
        const rtype::engine::SceneId scene = engine.getScenes().open();
        const rtype::ecs::entity_t entity = ecs.createEntity();
        ecs.addComponent<rtype::engine::SceneMember>(entity, scene);
        ecs.addComponent<KeyCapture>(entity, KeyCapture{.onKey = [this](Key key) { given = key; }});
    }

    void frame(std::set<Key> held)
    {
        input.keys = std::move(held);
        engine.update(1.0f / 60.0f);
    }
};

} // namespace

TEST_CASE("The capture takes the first key pressed, not the one held when it starts", "[settings]")
{
    Capture capture;

    capture.frame({Key::Enter}); // the key that opened it
    capture.frame({Key::Enter});
    CHECK_FALSE(capture.given);

    capture.frame({Key::Enter, Key::K});
    CHECK(capture.given == Key::K);

    capture.given.reset();
    capture.frame({Key::L});
    CHECK_FALSE(capture.given); // given once
}

TEST_CASE("The capture never takes the key that cancels it", "[settings]")
{
    Capture capture;
    capture.frame({});

    capture.frame({CancelKey});

    CHECK_FALSE(capture.given);
}
