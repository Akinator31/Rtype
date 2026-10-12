#pragma once

namespace rtype::engine
{

/// A control of the virtual gamepad that the engine reads, whatever the device: the keyboard and every real
/// gamepad are mapped onto it (see Controls). A control has no meaning in itself: the game decides what each
/// one does, for instance that FaceDown fires.
///
/// The buttons are named after their position, like input::GamepadButton.
enum class Control {
    Up,
    Down,
    Left,
    Right,
    /// Bottom face button: A on Xbox, Cross on PlayStation.
    FaceDown,
    /// Right face button: B on Xbox, Circle on PlayStation.
    FaceRight,
    /// Left face button: X on Xbox, Square on PlayStation.
    FaceLeft,
    /// Top face button: Y on Xbox, Triangle on PlayStation.
    FaceUp,
    /// Front shoulder button: LB on Xbox, L1 on PlayStation.
    LeftShoulder,
    /// Front shoulder button: RB on Xbox, R1 on PlayStation.
    RightShoulder,
    /// Back shoulder trigger: LT on Xbox, L2 on PlayStation.
    LeftTrigger,
    /// Back shoulder trigger: RT on Xbox, R2 on PlayStation.
    RightTrigger,
    /// Right centre button: Menu on Xbox, Start or Options on PlayStation.
    Start,
    /// Left centre button: View on Xbox, Select or Share on PlayStation.
    Select,
};

} // namespace rtype::engine
