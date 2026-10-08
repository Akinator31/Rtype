#pragma once

#include "rtype/input/ActionMap.hpp"

namespace rtype::engine
{

/// What the player can do in the interface. Separate from the game actions: the same key can mean something
/// else in a menu, like Escape, which opens the menu in game and goes back in a menu.
enum class Navigation {
    /// Focuses the previous widget.
    Up,
    /// Focuses the next widget.
    Down,
    /// Decreases the value of the focused widget (slider, choice, toggle).
    Left,
    /// Increases the value of the focused widget.
    Right,
    /// Activates the focused widget.
    Confirm,
    /// Goes back from the current scene.
    Back,
    /// Activates the widget under the mouse pointer. Separate from Confirm: a click outside of the widgets
    /// must
    /// not activate the focused one.
    Click,
};

/// Default controls of the interface: arrows, Enter to confirm and Escape to go back on the keyboard; D-pad
/// or left stick, bottom face button to confirm, right face button or Start to go back on the first gamepad;
/// left button to click on the mouse. Keys that type text are left out, so that they can be used in text
/// fields.
[[nodiscard]] input::ActionMap<Navigation> defaultNavigationControls();

} // namespace rtype::engine
