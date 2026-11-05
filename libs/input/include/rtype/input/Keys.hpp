#pragma once

#include <optional>
#include <span>
#include <string_view>

namespace rtype::input
{

/// A keyboard key, named after its position on a US (QWERTY) keyboard.
///
/// Keys are physical positions, not characters: Key::W is the key left of E, which is labelled Z on a French
/// (AZERTY) keyboard. Default controls such as WASD therefore become ZQSD on AZERTY without any change.
enum class Key {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Up,
    Down,
    Left,
    Right,
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    Delete,
    Home,
    End,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

/// Every key, in the order of Key: to look for the one the player presses.
[[nodiscard]] std::span<const Key> allKeys();
/// The name of a key, as in Key: "A", "Num1", "Space", "LeftShift", "F11". Meant to be saved, not shown: the
/// player's keyboard may name it otherwise (see keyLabel()).
[[nodiscard]] std::string_view keyName(Key key);
/// The key of a name given by keyName(), or nothing for another text.
[[nodiscard]] std::optional<Key> keyFromName(std::string_view name);

/// A gamepad button, named after its position so that it means the same on every brand of gamepad.
enum class GamepadButton {
    /// Bottom face button: A on Xbox, Cross on PlayStation.
    FaceDown,
    /// Right face button: B on Xbox, Circle on PlayStation.
    FaceRight,
    /// Left face button: X on Xbox, Square on PlayStation.
    FaceLeft,
    /// Top face button: Y on Xbox, Triangle on PlayStation.
    FaceUp,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    /// Front shoulder button: LB on Xbox, L1 on PlayStation.
    LeftBumper,
    /// Front shoulder button: RB on Xbox, R1 on PlayStation.
    RightBumper,
    /// Back shoulder trigger read as a button: LT on Xbox, L2 on PlayStation.
    LeftTrigger,
    /// Back shoulder trigger read as a button: RT on Xbox, R2 on PlayStation.
    RightTrigger,
    /// Left centre button: View on Xbox, Select or Share on PlayStation.
    Select,
    /// Right centre button: Menu on Xbox, Start or Options on PlayStation.
    Start,
    /// Left stick pressed.
    LeftThumb,
    /// Right stick pressed.
    RightThumb,
};

/// A mouse button.
enum class MouseButton {
    Left,
    Right,
    /// The wheel, pressed.
    Middle,
    /// Side button of some mice, usually "back" in a browser.
    Back,
    /// Side button of some mice, usually "forward" in a browser.
    Forward,
};

/// An analog input of a gamepad.
enum class GamepadAxis {
    /// Left stick, from -1 (left) to 1 (right).
    LeftX,
    /// Left stick, from -1 (up) to 1 (down).
    LeftY,
    /// Right stick, from -1 (left) to 1 (right).
    RightX,
    /// Right stick, from -1 (up) to 1 (down).
    RightY,
    /// Left back trigger, from 0 (released) to 1 (fully pressed).
    LeftTrigger,
    /// Right back trigger, from 0 (released) to 1 (fully pressed).
    RightTrigger,
};

} // namespace rtype::input
