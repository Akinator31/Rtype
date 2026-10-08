// The ActionMap is tested with a fake input source: no window, no real device.

#include "FakeInput.hpp"
#include "rtype/input/ActionMap.hpp"
#include "rtype/input/Binding.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rtype::input;

namespace
{

enum class Action {
    MoveUp,
    Shoot,
};

} // namespace

TEST_CASE("An action without bindings is never active", "[input]")
{
    ActionMap<Action> actions;
    FakeInput input;

    actions.update(input);

    REQUIRE_FALSE(actions.isActive(Action::Shoot));
    REQUIRE(actions.strength(Action::Shoot) == 0.0f);
}

TEST_CASE("An action is active while one of its keys is held", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    actions.bind(Action::Shoot, Binding::key(Key::Enter));
    FakeInput input;

    input.keys = {Key::Enter};
    actions.update(input);
    REQUIRE(actions.isActive(Action::Shoot));
    REQUIRE(actions.strength(Action::Shoot) == 1.0f);

    input.keys.clear();
    actions.update(input);
    REQUIRE_FALSE(actions.isActive(Action::Shoot));
}

TEST_CASE("Actions keep the state read at the last update", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    FakeInput input;

    input.keys = {Key::Space};
    REQUIRE_FALSE(actions.isActive(Action::Shoot));
}

TEST_CASE("A press and a release are reported for one update only", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    FakeInput input;

    input.keys = {Key::Space};
    actions.update(input);
    REQUIRE(actions.wasPressed(Action::Shoot));

    actions.update(input);
    REQUIRE(actions.isActive(Action::Shoot));
    REQUIRE_FALSE(actions.wasPressed(Action::Shoot));

    input.keys.clear();
    actions.update(input);
    REQUIRE(actions.wasReleased(Action::Shoot));

    actions.update(input);
    REQUIRE_FALSE(actions.wasReleased(Action::Shoot));
}

TEST_CASE("Gamepad buttons only count on the bound gamepad", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::gamepadButton(GamepadButton::FaceDown, 1));
    FakeInput input;

    input.buttons = {{0, GamepadButton::FaceDown}};
    actions.update(input);
    REQUIRE_FALSE(actions.isActive(Action::Shoot));

    input.buttons = {{1, GamepadButton::FaceDown}};
    actions.update(input);
    REQUIRE(actions.isActive(Action::Shoot));
}

TEST_CASE("A stick inside the dead zone does nothing", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));
    FakeInput input;

    input.axes[{0, GamepadAxis::LeftY}] = -0.2f;
    actions.update(input);

    REQUIRE_FALSE(actions.isActive(Action::MoveUp));
}

TEST_CASE("A stick outside the dead zone is scaled from 0 to 1", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));
    FakeInput input;

    // Default dead zone of 0.25: 0.625 is halfway between the dead zone and the end of the range
    input.axes[{0, GamepadAxis::LeftY}] = -0.625f;
    actions.update(input);
    REQUIRE(actions.strength(Action::MoveUp) == 0.5f);

    input.axes[{0, GamepadAxis::LeftY}] = -1.0f;
    actions.update(input);
    REQUIRE(actions.strength(Action::MoveUp) == 1.0f);
}

TEST_CASE("A stick pushed the other way does not trigger the action", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));
    FakeInput input;

    input.axes[{0, GamepadAxis::LeftY}] = 1.0f;
    actions.update(input);

    REQUIRE_FALSE(actions.isActive(Action::MoveUp));
}

TEST_CASE("The strongest input gives the strength of the action", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::key(Key::Up));
    actions.bind(Action::MoveUp, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));
    FakeInput input;

    input.keys = {Key::Up};
    input.axes[{0, GamepadAxis::LeftY}] = -0.625f;
    actions.update(input);

    REQUIRE(actions.strength(Action::MoveUp) == 1.0f);
}

TEST_CASE("Bindings can be changed", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    REQUIRE(actions.bindings(Action::Shoot).size() == 1);

    actions.unbind(Action::Shoot, Binding::key(Key::Space));
    actions.bind(Action::Shoot, Binding::key(Key::J));
    FakeInput input;

    input.keys = {Key::Space};
    actions.update(input);
    REQUIRE_FALSE(actions.isActive(Action::Shoot));

    input.keys = {Key::J};
    actions.update(input);
    REQUIRE(actions.isActive(Action::Shoot));

    actions.clear(Action::Shoot);
    REQUIRE(actions.bindings(Action::Shoot).empty());
}

TEST_CASE("A held action repeats after a delay, then at regular intervals", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::key(Key::Up));
    actions.setRepeat(0.5f, 0.25f);
    FakeInput input;
    input.keys = {Key::Up};

    // The press itself counts as the first repeat
    actions.update(input, 0.0f);
    REQUIRE(actions.wasRepeated(Action::MoveUp));

    // Nothing until the delay is over
    actions.update(input, 0.25f);
    REQUIRE_FALSE(actions.wasRepeated(Action::MoveUp));
    actions.update(input, 0.125f);
    REQUIRE_FALSE(actions.wasRepeated(Action::MoveUp));

    // Held for 0.5 s: first repeat, then one every 0.25 s
    actions.update(input, 0.125f);
    REQUIRE(actions.wasRepeated(Action::MoveUp));
    actions.update(input, 0.125f);
    REQUIRE_FALSE(actions.wasRepeated(Action::MoveUp));
    actions.update(input, 0.125f);
    REQUIRE(actions.wasRepeated(Action::MoveUp));
}

TEST_CASE("Actions do not repeat without the elapsed time", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::key(Key::Up));
    FakeInput input;
    input.keys = {Key::Up};

    actions.update(input);
    REQUIRE(actions.wasRepeated(Action::MoveUp));
    for (int frame = 0; frame < 100; ++frame) {
        actions.update(input);
        REQUIRE_FALSE(actions.wasRepeated(Action::MoveUp));
    }
}

TEST_CASE("The held duration is kept on the update where the action is released", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::key(Key::Space));
    FakeInput input;

    input.keys = {Key::Space};
    actions.update(input, 0.5f);
    REQUIRE(actions.heldDuration(Action::Shoot) == 0.0f);
    actions.update(input, 0.5f);
    actions.update(input, 0.25f);
    REQUIRE(actions.heldDuration(Action::Shoot) == 0.75f);

    // Released: a charged shot can read how long it was charged
    input.keys.clear();
    actions.update(input, 0.5f);
    REQUIRE(actions.wasReleased(Action::Shoot));
    REQUIRE(actions.heldDuration(Action::Shoot) == 0.75f);

    actions.update(input, 0.5f);
    REQUIRE(actions.heldDuration(Action::Shoot) == 0.0f);
}

TEST_CASE("A repeat falling exactly on an update is not lost to rounding", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::MoveUp, Binding::key(Key::Up));
    actions.setRepeat(0.4f, 0.1f);
    FakeInput input;
    input.keys = {Key::Up};

    actions.update(input, 0.0f);
    actions.update(input, 0.4f);
    REQUIRE(actions.wasRepeated(Action::MoveUp));

    // In float, (0.5 - 0.4) / 0.1 is slightly below 1: the second repeat must still happen at 0.5 s
    actions.update(input, 0.1f);
    REQUIRE(actions.wasRepeated(Action::MoveUp));
}

TEST_CASE("Mouse buttons can trigger actions", "[input]")
{
    ActionMap<Action> actions;
    actions.bind(Action::Shoot, Binding::mouseButton(MouseButton::Left));
    FakeInput input;

    input.mouseButtons = {MouseButton::Right};
    actions.update(input);
    REQUIRE_FALSE(actions.isActive(Action::Shoot));

    input.mouseButtons = {MouseButton::Left};
    actions.update(input);
    REQUIRE(actions.wasPressed(Action::Shoot));
}
