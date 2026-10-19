#include "rtype/game/ShipMovement.hpp"

#include "rtype/game/Constants.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::game
{

physics::Velocity shipVelocity(PlayerInput input, const Rules &rules)
{
    // A diagonal is as fast as a straight line
    const float diagonal = 1.0f / std::sqrt(2.0f);
    const PlayerInput::Direction direction = input.direction();
    const float scale = direction.x != 0 && direction.y != 0 ? diagonal : 1.0f;
    return {static_cast<float>(direction.x) * rules.shipSpeed * scale,
            static_cast<float>(direction.y) * rules.shipSpeed * scale};
}

ecs::Position moveShip(ecs::Position position, PlayerInput input, const Rules &rules)
{
    const physics::Velocity velocity = shipVelocity(input, rules);
    // As physics::MovementSystem, with the elapsed time of a tick
    position.x += velocity.x * TickDuration;
    position.y += velocity.y * TickDuration;
    // As physics::BoundsSystem, with the zone of the ship (ConfinedToBounds from 0 to the size of the game,
    // spawnPlayerShip()) and its box centred on it: the whole box stays inside
    position.x = std::clamp(position.x, rules.shipHalfWidth, GameWidth - rules.shipHalfWidth);
    position.y = std::clamp(position.y, rules.shipHalfHeight, GameHeight - rules.shipHalfHeight);
    return position;
}

} // namespace rtype::game
