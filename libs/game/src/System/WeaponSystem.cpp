#include "rtype/game/System/WeaponSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/Hitbox.hpp"

namespace rtype::game
{

namespace
{

/// Counts down a reload or a cooldown. Returns true when it is over.
bool tickDown(std::uint32_t &ticks)
{
    if (ticks > 0) {
        --ticks;
    }
    return ticks == 0;
}

/// Spends one round, and starts reloading on the last one.
void spendRound(Armed &armed, const WeaponDefinition &weapon)
{
    if (weapon.magazineSize == InfiniteAmmo) {
        return;
    }
    --armed.ammo;
    if (armed.ammo > 0) {
        return;
    }
    armed.reloadLeft = weapon.reloadTicks;
    if (weapon.reloadTicks == 0) {
        armed.ammo = weapon.magazineSize;
    }
}

} // namespace

WeaponSystem::WeaponSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _weapons(use<Armed>(caches)), _ships(use<PlayerShip>(caches)),
      _enemies(use<Enemy>(caches)), _positions(use<ecs::Position>(caches)),
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

        tickDown(armed.cooldown);
        if (armed.reloadLeft > 0 && tickDown(armed.reloadLeft)) {
            armed.ammo = weapon.magazineSize;
        }

        const bool ready = armed.cooldown == 0 && armed.reloadLeft == 0;
        if (!ready || !wantsToFire(entity)) {
            continue;
        }
        armed.cooldown = weapon.fireInterval;
        spendRound(armed, weapon);
        fire(entity, armed.weapon);
    }
}

bool WeaponSystem::wantsToFire(ecs::entity_t entity) const
{
    if (_ships.has(entity)) {
        return _context.players[_ships.get(entity).slot].input.isHeld(PlayerInput::Fire);
    }
    const float x = _positions.get(entity).x;
    return _enemies.has(entity) && x >= 0.0f && x <= GameWidth;
}

void WeaponSystem::fire(ecs::entity_t entity, const std::string &weapon)
{
    const std::uint8_t owner = _ships.has(entity) ? _ships.get(entity).slot : NoOwner;
    const float forward = owner < MaxPlayers ? 1.0f : -1.0f;
    const ecs::Position position = _positions.get(entity);
    const float noseX = position.x + forward * halfSizeOf(_colliders.get(entity)).x;
    _ecs.defer([this, weapon, owner, noseX, y = position.y] {
        spawnWeaponShot(_ecs, _context, weapon, owner, noseX, y);
    });
}

} // namespace rtype::game
