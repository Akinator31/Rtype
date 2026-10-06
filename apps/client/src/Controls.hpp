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
};

/// Default controls: arrows or WASD (ZQSD on AZERTY) and Space on the keyboard, D-pad or left stick and the
/// bottom face button or right trigger on the first gamepad.
[[nodiscard]] input::ActionMap<Action> defaultControls();

} // namespace rtype::client
