#pragma once

#include "rtype/input/Keys.hpp"

namespace rtype::input
{

/// Current state of the input devices: which keys and buttons are held, where the sticks are.
///
/// The game never reads devices directly: it reads actions from an ActionMap, which queries an InputSource.
/// Devices reads the real keyboard and gamepads; tests use a fake source, and other sources (VR controllers)
/// can be added without changing the game.
class InputSource
{
public:
    virtual ~InputSource() = default;

    /// Tells whether a keyboard key is held.
    [[nodiscard]] virtual bool isKeyDown(Key key) const = 0;
    /// Tells whether a gamepad button is held. False if this gamepad is not connected.
    /// @param gamepad Index of the gamepad, 0 for the first one.
    [[nodiscard]] virtual bool isGamepadButtonDown(int gamepad, GamepadButton button) const = 0;
    /// Position of a gamepad stick or trigger, in the range described by GamepadAxis. 0 if this gamepad is
    /// not connected.
    /// @param gamepad Index of the gamepad, 0 for the first one.
    [[nodiscard]] virtual float gamepadAxis(int gamepad, GamepadAxis axis) const = 0;
};

} // namespace rtype::input
