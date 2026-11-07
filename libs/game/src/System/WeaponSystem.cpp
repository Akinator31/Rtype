#include "rtype/game/System/WeaponSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/game/Weapon/WeaponFiring.hpp"

namespace rtype::game
{

WeaponSystem::WeaponSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _weapons(use<Armed>(caches)), _ships(use<PlayerShip>(caches)),
      _enemies(use<Enemy>(caches)), _orders(use<EnemyOrders>(caches)), _positions(use<ecs::Position>(caches)),
      _colliders(use<physics::Collider>(caches))
{
}

void WeaponSystem::update(float)
{
    for (ecs::entity_t entity = 0; entity < _weapons.size(); ++entity) {
        if (!_weapons.has(entity) || !_positions.has(entity) || !_colliders.has(entity) ||
            !_ecs.isAlive(entity)) {
            continue;
        }
        Armed &armed = _weapons.get(entity);
        const WeaponDefinition &weapon = _context.weapons.getDefinition(armed.weapon);

        // The same steps as the prediction of the client (WeaponFiring.hpp)
        if (stepWeapon(armed, weapon, wantsToFire(entity))) {
            fire(entity, armed.weapon);
        }
    }
}

bool WeaponSystem::wantsToFire(ecs::entity_t entity) const
{
    if (_ships.has(entity)) {
        return _context.players[_ships.get(entity).slot].input.isHeld(PlayerInput::Fire);
    }
    if (_orders.has(entity)) {
        return _orders.get(entity).fire;
    }
    const float x = _positions.get(entity).x;
    return _enemies.has(entity) && x >= 0.0f && x <= GameWidth;
}

void WeaponSystem::fire(ecs::entity_t entity, const std::string &weapon)
{
    const std::uint8_t owner = _ships.has(entity) ? _ships.get(entity).slot : NoOwner;
    const ecs::Position position = _positions.get(entity);
    if (owner < MaxPlayers) {
        // The same shot as the prediction of the client (WeaponFiring.hpp)
        const WeaponDefinition &definition = _context.weapons.getDefinition(weapon);
        const float halfWidth =
            halfSizeOf(_context.projectiles.getDefinition(definition.projectile).collider).x;
        const ShotLaunch launch = playerShot(definition, halfWidth, owner, position,
                                             _context.players[owner].input.aim(), _context.rules);
        _ecs.defer([this, weapon, owner, launch] { spawnShot(_ecs, _context, weapon, owner, launch); });
        return;
    }
    const float noseX = position.x - halfSizeOf(_colliders.get(entity)).x;
    if (_orders.has(entity) && _orders.get(entity).target) {
        const ecs::Position target = *_orders.get(entity).target;
        _ecs.defer([this, weapon, owner, noseX, y = position.y, target] {
            spawnAimedShot(_ecs, _context, weapon, owner, noseX, y, target.x, target.y);
        });
        return;
    }
    _ecs.defer([this, weapon, owner, noseX, y = position.y] {
        spawnWeaponShot(_ecs, _context, weapon, owner, noseX, y);
    });
}

} // namespace rtype::game
