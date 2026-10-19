#include "rtype/game/System/PlayerControlSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/ShipMovement.hpp"

namespace rtype::game
{

PlayerControlSystem::PlayerControlSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _ships(use<PlayerShip>(caches)),
      _velocities(use<physics::Velocity>(caches)), _positions(use<ecs::Position>(caches))
{
}

void PlayerControlSystem::update(float)
{
    const Rules &rules = _context.rules;

    for (ecs::entity_t entity = 0; entity < _ships.size(); ++entity) {
        if (!_ships.has(entity) || !_velocities.has(entity) || !_positions.has(entity) ||
            !_ecs.isAlive(entity)) {
            continue;
        }
        PlayerShip &ship = _ships.get(entity);
        const PlayerInput input = _context.players[ship.slot].input;

        // The same velocity as the prediction of the client (ShipMovement.hpp)
        _velocities.get(entity) = shipVelocity(input, rules);

        if (ship.fireCooldown > 0) {
            --ship.fireCooldown;
        }
        if (input.isHeld(PlayerInput::Fire) && ship.fireCooldown == 0) {
            ship.fireCooldown = rules.fireInterval;
            // From the nose of the ship. Created after the systems: it would grow the caches being read
            const ecs::Position position = _positions.get(entity);
            const float x = position.x + rules.shipHalfWidth + rules.shotHalfWidth;
            _ecs.defer(
                [this, slot = ship.slot, x, y = position.y] { spawnPlayerShot(_ecs, _context, slot, x, y); });
        }
    }
}

} // namespace rtype::game
