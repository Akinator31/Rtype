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

/// Default controls: arrows or WASD (ZQSD on AZERTY), Space and Escape on the keyboard; D-pad or left stick,
/// bottom face button or right trigger, and Start on the first gamepad.
[[nodiscard]] input::ActionMap<Action> defaultControls();

} // namespace rtype::client
