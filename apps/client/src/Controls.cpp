#include "Controls.hpp"

namespace rtype::client
{

input::ActionMap<Action> defaultControls()
{
    using input::AxisDirection;
    using input::Binding;
    using input::GamepadAxis;
    using input::GamepadButton;
    using input::Key;

    input::ActionMap<Action> controls;

    controls.bind(Action::MoveUp, Binding::key(Key::Up));
    controls.bind(Action::MoveUp, Binding::key(Key::W));
    controls.bind(Action::MoveUp, Binding::gamepadButton(GamepadButton::DPadUp));
    controls.bind(Action::MoveUp, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));

    controls.bind(Action::MoveDown, Binding::key(Key::Down));
    controls.bind(Action::MoveDown, Binding::key(Key::S));
    controls.bind(Action::MoveDown, Binding::gamepadButton(GamepadButton::DPadDown));
    controls.bind(Action::MoveDown, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Positive));

    controls.bind(Action::MoveLeft, Binding::key(Key::Left));
    controls.bind(Action::MoveLeft, Binding::key(Key::A));
    controls.bind(Action::MoveLeft, Binding::gamepadButton(GamepadButton::DPadLeft));
    controls.bind(Action::MoveLeft, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Negative));

    controls.bind(Action::MoveRight, Binding::key(Key::Right));
    controls.bind(Action::MoveRight, Binding::key(Key::D));
    controls.bind(Action::MoveRight, Binding::gamepadButton(GamepadButton::DPadRight));
    controls.bind(Action::MoveRight, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Positive));

    controls.bind(Action::Shoot, Binding::key(Key::Space));
    controls.bind(Action::Shoot, Binding::gamepadButton(GamepadButton::FaceDown));
    controls.bind(Action::Shoot, Binding::gamepadAxis(GamepadAxis::RightTrigger, AxisDirection::Positive));

    controls.bind(Action::Menu, Binding::key(Key::Escape));
    controls.bind(Action::Menu, Binding::gamepadButton(GamepadButton::Start));

    return controls;
}

input::ActionMap<MenuAction> defaultMenuControls()
{
    using input::AxisDirection;
    using input::Binding;
    using input::GamepadAxis;
    using input::GamepadButton;
    using input::Key;

    input::ActionMap<MenuAction> controls;
    // A stick must be clearly pushed to move in a menu: a slight touch would skip an item
    controls.setDeadZone(0.5f);
    // Only keys that do not type text: menus can contain text fields, where letters, Space and Backspace edit
    // the text instead of moving or confirming

    controls.bind(MenuAction::Up, Binding::key(Key::Up));
    controls.bind(MenuAction::Up, Binding::gamepadButton(GamepadButton::DPadUp));
    controls.bind(MenuAction::Up, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));

    controls.bind(MenuAction::Down, Binding::key(Key::Down));
    controls.bind(MenuAction::Down, Binding::gamepadButton(GamepadButton::DPadDown));
    controls.bind(MenuAction::Down, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Positive));

    controls.bind(MenuAction::Left, Binding::key(Key::Left));
    controls.bind(MenuAction::Left, Binding::gamepadButton(GamepadButton::DPadLeft));
    controls.bind(MenuAction::Left, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Negative));

    controls.bind(MenuAction::Right, Binding::key(Key::Right));
    controls.bind(MenuAction::Right, Binding::gamepadButton(GamepadButton::DPadRight));
    controls.bind(MenuAction::Right, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Positive));

    controls.bind(MenuAction::Confirm, Binding::key(Key::Enter));
    controls.bind(MenuAction::Confirm, Binding::gamepadButton(GamepadButton::FaceDown));

    controls.bind(MenuAction::Back, Binding::key(Key::Escape));
    controls.bind(MenuAction::Back, Binding::gamepadButton(GamepadButton::FaceRight));
    controls.bind(MenuAction::Back, Binding::gamepadButton(GamepadButton::Start));

    return controls;
}

} // namespace rtype::client
