#include "rtype/game/System/PlayerControlSystem.hpp"

#include "rtype/game/ShipMovement.hpp"

namespace rtype::game
{

PlayerControlSystem::PlayerControlSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _ships(use<PlayerShip>(caches)),
      _velocities(use<physics::Velocity>(caches))
{
}

void PlayerControlSystem::update(float)
{
    for (ecs::entity_t entity = 0; entity < _ships.size(); ++entity) {
        if (!_ships.has(entity) || !_velocities.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        const PlayerInput input = _context.players[_ships.get(entity).slot].input;
        // The same velocity as the prediction of the client (ShipMovement.hpp)
        _velocities.get(entity) = shipVelocity(input, _context.rules);
    }
}

} // namespace rtype::game
