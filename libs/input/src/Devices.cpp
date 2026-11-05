#include "rtype/input/Devices.hpp"

#include <raylib.h>
// The names of the keys on the layout of the player come from GLFW, which raylib is built on
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>

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
    case Key::Delete:
        return KEY_DELETE;
    case Key::Home:
        return KEY_HOME;
    case Key::End:
        return KEY_END;
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

int toRaylib(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return MOUSE_BUTTON_LEFT;
    case MouseButton::Right:
        return MOUSE_BUTTON_RIGHT;
    case MouseButton::Middle:
        return MOUSE_BUTTON_MIDDLE;
    case MouseButton::Back:
        return MOUSE_BUTTON_SIDE;
    case MouseButton::Forward:
        return MOUSE_BUTTON_EXTRA;
    }
    return MOUSE_BUTTON_LEFT;
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

MousePosition Devices::mousePosition() const
{
    const ::Vector2 position = GetMousePosition();
    return {position.x, position.y};
}

bool Devices::isMouseButtonDown(MouseButton button) const
{
    return IsMouseButtonDown(toRaylib(button));
}

float Devices::mouseWheel() const
{
    // Vertical wheel only: the horizontal one of some mice and touchpads is not used
    return GetMouseWheelMoveV().y;
}

std::u32string Devices::takeTypedText()
{
    std::u32string text;
    for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
        text += static_cast<char32_t>(character);
    }
    return text;
}

std::string Devices::clipboardText() const
{
    const char *text = GetClipboardText();
    return text == nullptr ? std::string() : std::string(text);
}

void Devices::setClipboardText(const std::string &text)
{
    SetClipboardText(text.c_str());
}

// raylib's key codes are GLFW's
std::string keyLabel(Key key)
{
    const char *name = glfwGetKeyName(toRaylib(key), 0);
    std::string label = name != nullptr ? name : "";
    std::ranges::transform(label, label.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return label;
}

} // namespace rtype::input
