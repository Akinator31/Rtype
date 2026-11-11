// The virtual gamepad and pointer over the devices (the controllers of a VR headset), with a fake source.

#include "FakeInput.hpp"
#include "rtype/input/ActionMap.hpp"
#include "rtype/input/VirtualInput.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace rtype::input;

TEST_CASE("The virtual gamepad is gamepad 0, held together with the real one", "[input]")
{
    FakeInput devices;
    VirtualInput input(devices);
    REQUIRE_FALSE(input.isGamepadButtonDown(0, GamepadButton::FaceDown));

    GamepadState gamepad;
    gamepad.button(GamepadButton::FaceDown) = true;
    gamepad.axis(GamepadAxis::LeftX) = -0.4f;
    input.setGamepad(gamepad);
    REQUIRE(input.isGamepadButtonDown(0, GamepadButton::FaceDown));
    REQUIRE_FALSE(input.isGamepadButtonDown(1, GamepadButton::FaceDown));
    REQUIRE(input.gamepadAxis(0, GamepadAxis::LeftX) == -0.4f);

    // The real one still works, and the farthest axis wins
    devices.buttons.insert({0, GamepadButton::FaceRight});
    devices.axes[{0, GamepadAxis::LeftX}] = 0.9f;
    devices.axes[{1, GamepadAxis::LeftY}] = 0.5f;
    REQUIRE(input.isGamepadButtonDown(0, GamepadButton::FaceRight));
    REQUIRE(input.gamepadAxis(0, GamepadAxis::LeftX) == 0.9f);
    REQUIRE(input.gamepadAxis(1, GamepadAxis::LeftY) == 0.5f);
}

TEST_CASE("The bindings of the player read the virtual gamepad", "[input]")
{
    enum class Action { Fire };
    FakeInput devices;
    VirtualInput input(devices);
    ActionMap<Action> actions;
    actions.bind(Action::Fire, Binding::gamepadButton(GamepadButton::FaceDown));
    GamepadState gamepad;
    gamepad.button(GamepadButton::FaceDown) = true;
    input.setGamepad(gamepad);
    actions.update(input);
    REQUIRE(actions.wasPressed(Action::Fire));
}

TEST_CASE("A pointer replaces the mouse while it is given", "[input]")
{
    FakeInput devices;
    devices.mouse = {10.0f, 20.0f};
    devices.mouseButtons.insert(MouseButton::Right);
    devices.wheel = 2.0f;
    VirtualInput input(devices);
    REQUIRE(input.mousePosition().x == 10.0f);
    REQUIRE(input.isMouseButtonDown(MouseButton::Right));

    input.setPointer(PointerState{.position = {960.0f, 540.0f}, .held = true, .wheel = -1.0f});
    REQUIRE(input.mousePosition().x == 960.0f);
    REQUIRE(input.isMouseButtonDown(MouseButton::Left));
    REQUIRE_FALSE(input.isMouseButtonDown(MouseButton::Right));
    REQUIRE(input.mouseWheel() == -1.0f);

    input.setPointer(std::nullopt);
    REQUIRE(input.mousePosition().y == 20.0f);
    REQUIRE(input.mouseWheel() == 2.0f);
}

TEST_CASE("The keyboard and the text typed are those of the devices", "[input]")
{
    FakeInput devices;
    devices.keys.insert(Key::Space);
    devices.typed = U"é";
    VirtualInput input(devices);
    REQUIRE(input.isKeyDown(Key::Space));
    REQUIRE(input.takeTypedText() == U"é");
    input.setClipboardText("copied");
    REQUIRE(devices.clipboard == "copied");
}
