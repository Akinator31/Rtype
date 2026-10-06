#pragma once

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
