#pragma once

#include "rtype/input/InputSource.hpp"
#include "rtype/input/Keys.hpp"

#include <optional>
#include <variant>

namespace rtype::input
{

/// Which half of an axis triggers a binding: for instance the left stick pushed up is LeftY, Negative.
enum class AxisDirection {
    Negative,
    Positive,
};

/// One physical input that triggers an action: a key, a gamepad button, or a direction of a gamepad axis.
class Binding
{
private:
    struct KeyInput {
        Key key;
        friend bool operator==(const KeyInput &, const KeyInput &) = default;
    };
    struct ButtonInput {
        GamepadButton button;
        int gamepad;
        friend bool operator==(const ButtonInput &, const ButtonInput &) = default;
    };
    struct AxisInput {
        GamepadAxis axis;
        AxisDirection direction;
        int gamepad;
        friend bool operator==(const AxisInput &, const AxisInput &) = default;
    };
    struct MouseInput {
        MouseButton button;
        friend bool operator==(const MouseInput &, const MouseInput &) = default;
    };

    using Input = std::variant<KeyInput, ButtonInput, AxisInput, MouseInput>;
    Input _input;

    explicit Binding(Input input);

public:
    /// A keyboard key.
    [[nodiscard]] static Binding key(Key key);
    /// A gamepad button.
    /// @param gamepad Index of the gamepad, 0 for the first one.
    [[nodiscard]] static Binding gamepadButton(GamepadButton button, int gamepad = 0);
    /// One direction of a gamepad stick or trigger. Triggers only have a positive direction.
    /// @param gamepad Index of the gamepad, 0 for the first one.
    [[nodiscard]] static Binding gamepadAxis(GamepadAxis axis, AxisDirection direction, int gamepad = 0);
    /// A mouse button.
    [[nodiscard]] static Binding mouseButton(MouseButton button);

    /// The key of a keyboard binding, nothing for another input.
    [[nodiscard]] std::optional<Key> asKey() const;

    /// How strongly the input is held, from 0 (released) to 1 (fully pressed).
    ///
    /// Keys, gamepad buttons and mouse buttons are 0 or 1. Axes are 0 inside the dead zone, then grow
    /// progressively up to 1, so that a stick slightly off-centre does not move anything.
    /// @param deadZone Part of the axis range that is ignored, between 0 and 1.
    [[nodiscard]] float strength(const InputSource &source, float deadZone) const;

    friend bool operator==(const Binding &, const Binding &) = default;
};

} // namespace rtype::input
