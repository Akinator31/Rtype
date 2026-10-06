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

    return controls;
}

} // namespace rtype::client
