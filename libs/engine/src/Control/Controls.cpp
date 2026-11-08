#include "rtype/engine/Control/Controls.hpp"

namespace rtype::engine
{

Controls::Controls()
{
    bindDefaults();
}

void Controls::bindDefaults()
{
    using input::AxisDirection;
    using input::Binding;
    using input::GamepadAxis;
    using input::GamepadButton;
    using input::Key;

    bind(Control::Up, Binding::key(Key::Up));
    bind(Control::Up, Binding::key(Key::W));
    bind(Control::Up, Binding::gamepadButton(GamepadButton::DPadUp));
    bind(Control::Up, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Negative));

    bind(Control::Down, Binding::key(Key::Down));
    bind(Control::Down, Binding::key(Key::S));
    bind(Control::Down, Binding::gamepadButton(GamepadButton::DPadDown));
    bind(Control::Down, Binding::gamepadAxis(GamepadAxis::LeftY, AxisDirection::Positive));

    bind(Control::Left, Binding::key(Key::Left));
    bind(Control::Left, Binding::key(Key::A));
    bind(Control::Left, Binding::gamepadButton(GamepadButton::DPadLeft));
    bind(Control::Left, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Negative));

    bind(Control::Right, Binding::key(Key::Right));
    bind(Control::Right, Binding::key(Key::D));
    bind(Control::Right, Binding::gamepadButton(GamepadButton::DPadRight));
    bind(Control::Right, Binding::gamepadAxis(GamepadAxis::LeftX, AxisDirection::Positive));

    bind(Control::FaceDown, Binding::key(Key::Space));
    bind(Control::FaceDown, Binding::gamepadButton(GamepadButton::FaceDown));
    bind(Control::FaceRight, Binding::key(Key::LeftControl));
    bind(Control::FaceRight, Binding::gamepadButton(GamepadButton::FaceRight));
    bind(Control::FaceLeft, Binding::key(Key::LeftShift));
    bind(Control::FaceLeft, Binding::gamepadButton(GamepadButton::FaceLeft));
    bind(Control::FaceUp, Binding::key(Key::F));
    bind(Control::FaceUp, Binding::gamepadButton(GamepadButton::FaceUp));

    bind(Control::LeftShoulder, Binding::key(Key::Q));
    bind(Control::LeftShoulder, Binding::gamepadButton(GamepadButton::LeftBumper));
    bind(Control::RightShoulder, Binding::key(Key::E));
    bind(Control::RightShoulder, Binding::gamepadButton(GamepadButton::RightBumper));
    bind(Control::LeftTrigger, Binding::gamepadButton(GamepadButton::LeftTrigger));
    bind(Control::LeftTrigger, Binding::gamepadAxis(GamepadAxis::LeftTrigger, AxisDirection::Positive));
    bind(Control::RightTrigger, Binding::gamepadButton(GamepadButton::RightTrigger));
    bind(Control::RightTrigger, Binding::gamepadAxis(GamepadAxis::RightTrigger, AxisDirection::Positive));

    bind(Control::Start, Binding::key(Key::Escape));
    bind(Control::Start, Binding::key(Key::Enter));
    bind(Control::Start, Binding::gamepadButton(GamepadButton::Start));
    bind(Control::Select, Binding::key(Key::Tab));
    bind(Control::Select, Binding::gamepadButton(GamepadButton::Select));
}

void Controls::update(const input::InputSource &source, float elapsed)
{
    _controls.update(source, elapsed);
}

void Controls::bind(Control control, const input::Binding &binding)
{
    _controls.bind(control, binding);
}

void Controls::unbind(Control control, const input::Binding &binding)
{
    _controls.unbind(control, binding);
}

void Controls::clear(Control control)
{
    _controls.clear(control);
}

std::vector<input::Binding> Controls::getBindings(Control control) const
{
    return _controls.bindings(control);
}

void Controls::resetDefaults()
{
    for (int control = static_cast<int>(Control::Up); control <= static_cast<int>(Control::Select);
         ++control) {
        clear(static_cast<Control>(control));
    }
    bindDefaults();
}

void Controls::setDeadZone(float deadZone)
{
    _controls.setDeadZone(deadZone);
}

bool Controls::isDown(Control control) const
{
    return _controls.isActive(control);
}

bool Controls::wasPressed(Control control) const
{
    return _controls.wasPressed(control);
}

bool Controls::wasReleased(Control control) const
{
    return _controls.wasReleased(control);
}

bool Controls::wasRepeated(Control control) const
{
    return _controls.wasRepeated(control);
}

float Controls::getStrength(Control control) const
{
    return _controls.strength(control);
}

float Controls::getHeldDuration(Control control) const
{
    return _controls.heldDuration(control);
}

render::Vector2 Controls::getDirection() const
{
    return {getStrength(Control::Right) - getStrength(Control::Left),
            getStrength(Control::Down) - getStrength(Control::Up)};
}

} // namespace rtype::engine
