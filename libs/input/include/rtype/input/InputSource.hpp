#pragma once

#include "rtype/input/Keys.hpp"

#include <string>

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

    /// Characters typed since the previous frame, in the order they were typed, as Unicode code points.
    ///
    /// Unlike keys, these are characters: they follow the keyboard layout and its modifiers (Shift, AltGr,
    /// accents), so typing "é" on a French keyboard gives "é". The characters are consumed: a second call in
    /// the same frame returns nothing. Read them through a TextInput, once per frame.
    [[nodiscard]] virtual std::u32string takeTypedText() = 0;

    /// Text of the system clipboard, in UTF-8, to paste it. Empty if the clipboard holds no text.
    [[nodiscard]] virtual std::string clipboardText() const = 0;
    /// Replaces the text of the system clipboard, to copy text.
    /// @param text Text in UTF-8.
    virtual void setClipboardText(const std::string &text) = 0;
};

} // namespace rtype::input
