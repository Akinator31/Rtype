#include "rtype/game/System/WeaponSystem.hpp"

#include "rtype/game/Entities.hpp"

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

} // namespace

WeaponSystem::WeaponSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _weapons(use<Armed>(caches)), _ships(use<PlayerShip>(caches)),
      _positions(use<ecs::Position>(caches))
{
}

void WeaponSystem::update(float)
{
    for (ecs::entity_t entity = 0; entity < _weapons.size(); ++entity) {
        if (!_weapons.has(entity) || !_ships.has(entity) || !_positions.has(entity) ||
            !_ecs.isAlive(entity)) {
            continue;
        }
        Armed &armed = _weapons.get(entity);
        const WeaponDefinition &weapon = _context.weapons.getDefinition(armed.weapon);
        const std::uint8_t slot = _ships.get(entity).slot;

        tickDown(armed.cooldown);
        if (armed.reloadLeft > 0 && tickDown(armed.reloadLeft)) {
            armed.ammo = weapon.magazineSize;
        }

        const bool ready = armed.cooldown == 0 && armed.reloadLeft == 0;
        if (!ready || !_context.players[slot].input.isHeld(PlayerInput::Fire)) {
            continue;
        }
        armed.cooldown = weapon.fireInterval;
        if (weapon.magazineSize != InfiniteAmmo) {
            --armed.ammo;
            if (armed.ammo == 0) {
                armed.reloadLeft = weapon.reloadTicks;
                if (weapon.reloadTicks == 0) {
                    armed.ammo = weapon.magazineSize;
                }
            }
        }
        const ecs::Position position = _positions.get(entity);
        _ecs.defer([this, id = armed.weapon, slot, position] {
            spawnWeaponShot(_ecs, _context, id, slot, position.x, position.y);
        });
    }
}

} // namespace rtype::game
