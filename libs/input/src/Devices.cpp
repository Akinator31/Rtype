#include "rtype/input/Devices.hpp"

#include <raylib.h>

#include <algorithm>

namespace rtype::input
{

namespace
{

// Our enums have the same names as raylib's GamepadButton and GamepadAxis: raylib's values are used as ints

int toRaylib(Key key)
{
    const int index = static_cast<int>(key);
    if (key <= Key::Z) {
        return KEY_A + index;
    }
    if (key <= Key::Num9) {
        return KEY_ZERO + (index - static_cast<int>(Key::Num0));
    }
    if (key >= Key::F1) {
        return KEY_F1 + (index - static_cast<int>(Key::F1));
    }
    switch (key) {
    case Key::Up:
        return KEY_UP;
    case Key::Down:
        return KEY_DOWN;
    case Key::Left:
        return KEY_LEFT;
    case Key::Right:
        return KEY_RIGHT;
    case Key::Space:
        return KEY_SPACE;
    case Key::Enter:
        return KEY_ENTER;
    case Key::Escape:
        return KEY_ESCAPE;
    case Key::Tab:
        return KEY_TAB;
    case Key::Backspace:
        return KEY_BACKSPACE;
    case Key::LeftShift:
        return KEY_LEFT_SHIFT;
    case Key::RightShift:
        return KEY_RIGHT_SHIFT;
    case Key::LeftControl:
        return KEY_LEFT_CONTROL;
    case Key::RightControl:
        return KEY_RIGHT_CONTROL;
    case Key::LeftAlt:
        return KEY_LEFT_ALT;
    case Key::RightAlt:
        return KEY_RIGHT_ALT;
    default:
        return KEY_NULL;
    }
}

int toRaylib(GamepadButton button)
{
    switch (button) {
    case GamepadButton::FaceDown:
        return GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
    case GamepadButton::FaceRight:
        return GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
    case GamepadButton::FaceLeft:
        return GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
    case GamepadButton::FaceUp:
        return GAMEPAD_BUTTON_RIGHT_FACE_UP;
    case GamepadButton::DPadUp:
        return GAMEPAD_BUTTON_LEFT_FACE_UP;
    case GamepadButton::DPadDown:
        return GAMEPAD_BUTTON_LEFT_FACE_DOWN;
    case GamepadButton::DPadLeft:
        return GAMEPAD_BUTTON_LEFT_FACE_LEFT;
    case GamepadButton::DPadRight:
        return GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
    case GamepadButton::LeftBumper:
        return GAMEPAD_BUTTON_LEFT_TRIGGER_1;
    case GamepadButton::RightBumper:
        return GAMEPAD_BUTTON_RIGHT_TRIGGER_1;
    case GamepadButton::LeftTrigger:
        return GAMEPAD_BUTTON_LEFT_TRIGGER_2;
    case GamepadButton::RightTrigger:
        return GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
    case GamepadButton::Select:
        return GAMEPAD_BUTTON_MIDDLE_LEFT;
    case GamepadButton::Start:
        return GAMEPAD_BUTTON_MIDDLE_RIGHT;
    case GamepadButton::LeftThumb:
        return GAMEPAD_BUTTON_LEFT_THUMB;
    case GamepadButton::RightThumb:
        return GAMEPAD_BUTTON_RIGHT_THUMB;
    }
    return GAMEPAD_BUTTON_UNKNOWN;
}

int toRaylib(GamepadAxis axis)
{
    switch (axis) {
    case GamepadAxis::LeftX:
        return GAMEPAD_AXIS_LEFT_X;
    case GamepadAxis::LeftY:
        return GAMEPAD_AXIS_LEFT_Y;
    case GamepadAxis::RightX:
        return GAMEPAD_AXIS_RIGHT_X;
    case GamepadAxis::RightY:
        return GAMEPAD_AXIS_RIGHT_Y;
    case GamepadAxis::LeftTrigger:
        return GAMEPAD_AXIS_LEFT_TRIGGER;
    case GamepadAxis::RightTrigger:
        return GAMEPAD_AXIS_RIGHT_TRIGGER;
    }
    return GAMEPAD_AXIS_LEFT_X;
}

bool isTrigger(GamepadAxis axis)
{
    return axis == GamepadAxis::LeftTrigger || axis == GamepadAxis::RightTrigger;
}

bool isConnected(int gamepad)
{
    return gamepad >= 0 && IsGamepadAvailable(gamepad);
}

} // namespace

// The window is only required to make sure that one is open: raylib receives the input events through it
Devices::Devices(const render::Window & /*window*/) {}

bool Devices::isKeyDown(Key key) const
{
    return IsKeyDown(toRaylib(key));
}

bool Devices::isGamepadButtonDown(int gamepad, GamepadButton button) const
{
    return isConnected(gamepad) && IsGamepadButtonDown(gamepad, toRaylib(button));
}

float Devices::gamepadAxis(int gamepad, GamepadAxis axis) const
{
    if (!isConnected(gamepad)) {
        return 0.0f;
    }
    const float value = GetGamepadAxisMovement(gamepad, toRaylib(axis));
    // raylib gives triggers from -1 (released) to 1 (pressed): bring them to 0..1 like the documentation says
    return isTrigger(axis) ? std::clamp((value + 1.0f) / 2.0f, 0.0f, 1.0f) : std::clamp(value, -1.0f, 1.0f);
}

} // namespace rtype::input
