#include "rtype/input/Binding.hpp"

#include <algorithm>
#include <utility>

namespace rtype::input
{

Binding::Binding(std::variant<KeyInput, ButtonInput, AxisInput> input) : _input(std::move(input)) {}

Binding Binding::key(Key key)
{
    return Binding(KeyInput{key});
}

Binding Binding::gamepadButton(GamepadButton button, int gamepad)
{
    return Binding(ButtonInput{button, gamepad});
}

Binding Binding::gamepadAxis(GamepadAxis axis, AxisDirection direction, int gamepad)
{
    return Binding(AxisInput{axis, direction, gamepad});
}

float Binding::strength(const InputSource &source, float deadZone) const
{
    if (const auto *input = std::get_if<KeyInput>(&_input)) {
        return source.isKeyDown(input->key) ? 1.0f : 0.0f;
    }
    if (const auto *input = std::get_if<ButtonInput>(&_input)) {
        return source.isGamepadButtonDown(input->gamepad, input->button) ? 1.0f : 0.0f;
    }
    const auto &input = std::get<AxisInput>(_input);
    const float value = source.gamepadAxis(input.gamepad, input.axis);
    const float amount = input.direction == AxisDirection::Positive ? value : -value;
    if (amount <= deadZone) {
        return 0.0f;
    }
    // Start from 0 at the edge of the dead zone, so that the movement does not jump when leaving it
    return std::min((amount - deadZone) / (1.0f - deadZone), 1.0f);
}

} // namespace rtype::input
