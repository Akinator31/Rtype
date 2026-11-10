#include "rtype/input/VirtualInput.hpp"

#include <cmath>

namespace rtype::input
{

namespace
{

/// The gamepad the virtual one is: the first, which the bindings read by default.
constexpr int VirtualGamepad = 0;

} // namespace

bool VirtualInput::isGamepadButtonDown(int gamepad, GamepadButton button) const
{
    return _source.isGamepadButtonDown(gamepad, button) ||
           (gamepad == VirtualGamepad && _gamepad.button(button));
}

float VirtualInput::gamepadAxis(int gamepad, GamepadAxis axis) const
{
    const float real = _source.gamepadAxis(gamepad, axis);
    if (gamepad != VirtualGamepad) {
        return real;
    }
    const float added = _gamepad.axis(axis);
    return std::abs(added) > std::abs(real) ? added : real;
}

MousePosition VirtualInput::mousePosition() const
{
    return _pointer ? _pointer->position : _source.mousePosition();
}

bool VirtualInput::isMouseButtonDown(MouseButton button) const
{
    if (!_pointer) {
        return _source.isMouseButtonDown(button);
    }
    return button == MouseButton::Left && _pointer->held;
}

float VirtualInput::mouseWheel() const
{
    return _pointer ? _pointer->wheel : _source.mouseWheel();
}

} // namespace rtype::input
