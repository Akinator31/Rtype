#pragma once

#include "rtype/engine/Control/Control.hpp"
#include "rtype/input/ActionMap.hpp"
#include "rtype/input/Binding.hpp"
#include "rtype/input/InputSource.hpp"
#include "rtype/render/Types.hpp"

#include <vector>

namespace rtype::engine
{

/// The virtual gamepad: reads the keyboard and the real gamepads through rebindable bindings, and gives the
/// state of each Control. The game only reads controls, never keys nor buttons.
///
/// Default bindings, on the keyboard and the first gamepad:
/// - Up, Down, Left, Right: arrows and WASD (ZQSD on AZERTY); D-pad and left stick.
/// - FaceDown: Space. FaceRight: Left Control. FaceLeft: Left Shift. FaceUp: M.
/// - LeftShoulder: Q. RightShoulder: E. LeftTrigger: V. RightTrigger: gamepad only. Both triggers as a button
///   or as an axis.
/// - Start: Escape and Enter. Select: N.
class Controls
{
    input::ActionMap<Control> _controls;

    void bindDefaults();

public:
    /// Starts with the default bindings.
    Controls();

    /// Reads the input source and updates every control. The Engine calls it at the start of each update.
    /// @param elapsed Time since the previous update, in seconds, for getHeldDuration() and wasRepeated().
    void update(const input::InputSource &source, float elapsed = 0.0f);

    /// Adds an input that triggers the control. Adding the same input twice has no effect.
    void bind(Control control, const input::Binding &binding);
    /// Removes an input from the control. Does nothing if it was not bound to it.
    void unbind(Control control, const input::Binding &binding);
    /// Removes every input of the control, for instance before the player chooses new ones.
    void clear(Control control);
    [[nodiscard]] std::vector<input::Binding> getBindings(Control control) const;
    /// Puts every control back to its default bindings, those listed above.
    void resetDefaults();

    /// Sets the part of the stick and trigger range that is ignored, 0.25 by default.
    void setDeadZone(float deadZone);

    /// Tells whether the control is held.
    [[nodiscard]] bool isDown(Control control) const;
    /// True on the update where the control is pressed only.
    [[nodiscard]] bool wasPressed(Control control) const;
    /// True on the update where the control is released only.
    [[nodiscard]] bool wasReleased(Control control) const;
    /// True when the control is pressed, then again at regular intervals while it is held.
    [[nodiscard]] bool wasRepeated(Control control) const;
    /// How strongly the control is held, from 0 to 1: analog on sticks and triggers.
    [[nodiscard]] float getStrength(Control control) const;
    /// How long the control has been held, in seconds; on its release, how long it was held.
    [[nodiscard]] float getHeldDuration(Control control) const;

    /// The direction held, from -1 to 1 on each axis: x is Right minus Left, y is Down minus Up. Analog on
    /// the stick, -1, 0 or 1 on the keyboard. It is not normalized: diagonals are longer than 1.
    [[nodiscard]] render::Vector2 getDirection() const;
};

} // namespace rtype::engine
