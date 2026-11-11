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
        const PlayerSlot &player = _context.players[_ships.get(entity).slot];
        // The same velocity as the prediction of the client (ShipMovement.hpp)
        _velocities.get(entity) = shipVelocity(player.input, _context.rules, statFactor(player.stats.speed));
    }
}

} // namespace rtype::game
