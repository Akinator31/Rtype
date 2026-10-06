#pragma once

#include "rtype/input/ActionMap.hpp"

namespace rtype::client
{

/// What the player can do. These actions, not the keys, are what the client sends to the server.
enum class Action {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Shoot,
    Menu,
};

/// What the player can do in a menu. Menus have their own actions, separate from the game ones: the same key
/// can mean something else in a menu, like Escape, which opens the menu in game and goes back in a menu.
enum class MenuAction {
    Up,
    Down,
    Left,
    Right,
    /// Activates the selected item.
    Confirm,
    /// Goes back to the previous screen, or closes the menu.
    Back,
};

/// Default controls: arrows or WASD (ZQSD on AZERTY), Space and Escape on the keyboard; D-pad or left stick,
/// bottom face button or right trigger, and Start on the first gamepad.
[[nodiscard]] input::ActionMap<Action> defaultControls();

/// Default menu controls: arrows or WASD (ZQSD on AZERTY), Enter or Space to confirm, Escape or Backspace to
/// go back on the keyboard; D-pad or left stick, bottom face button to confirm, right face button or Start to
/// go back on the first gamepad.
[[nodiscard]] input::ActionMap<MenuAction> defaultMenuControls();

} // namespace rtype::client
