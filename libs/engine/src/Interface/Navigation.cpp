#include "rtype/engine/Interface/Navigation.hpp"

#include "rtype/input/Binding.hpp"

namespace rtype::engine
{

input::ActionMap<Navigation> defaultNavigationControls()
{
    using input::AxisDirection;
    using input::Binding;
    using input::GamepadAxis;
    using input::GamepadButton;
    using input::Key;

    input::ActionMap<Navigation> controls;
    // A stick must be clearly pushed to move in a menu: a slight touch would skip a widget
    controls.setDeadZone(0.5f);
    // Only keys that do not type text: menus can contain text fields, where letters, Space and Backspace edit
    // the text instead of moving or confirming

    controls.bind(Navigation::Up, Binding::key(Key::Up));
    controls.bind(Navigation::Up, Binding::gamepadButton(GamepadButton::DPadUp));
    controls.bind(Navigation::Up, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));

    controls.bind(Navigation::Down, Binding::key(Key::Down));
    controls.bind(Navigation::Down, Binding::gamepadButton(GamepadButton::DPadDown));
    controls.bind(Navigation::Down, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Positive));

    controls.bind(Navigation::Left, Binding::key(Key::Left));
    controls.bind(Navigation::Left, Binding::gamepadButton(GamepadButton::DPadLeft));
    controls.bind(Navigation::Left, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Negative));

    controls.bind(Navigation::Right, Binding::key(Key::Right));
    controls.bind(Navigation::Right, Binding::gamepadButton(GamepadButton::DPadRight));
    controls.bind(Navigation::Right, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Positive));

    controls.bind(Navigation::Confirm, Binding::key(Key::Enter));
    controls.bind(Navigation::Confirm, Binding::gamepadButton(GamepadButton::FaceDown));

    controls.bind(Navigation::Back, Binding::key(Key::Escape));
    controls.bind(Navigation::Back, Binding::gamepadButton(GamepadButton::FaceRight));
    controls.bind(Navigation::Back, Binding::gamepadButton(GamepadButton::Start));

    controls.bind(Navigation::Click, Binding::mouseButton(input::MouseButton::Left));

    return controls;
}

} // namespace rtype::engine
