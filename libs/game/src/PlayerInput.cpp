#include "rtype/game/PlayerInput.hpp"

namespace rtype::game
{

PlayerInput::Direction PlayerInput::direction() const
{
    Direction direction;
    direction.x = (isHeld(Right) ? 1 : 0) - (isHeld(Left) ? 1 : 0);
    direction.y = (isHeld(Down) ? 1 : 0) - (isHeld(Up) ? 1 : 0);
    return direction;
}

} // namespace rtype::game
