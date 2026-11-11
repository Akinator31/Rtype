#include "rtype/game/Weapon/WeaponFiring.hpp"

#include "rtype/game/Constants.hpp"
#include "rtype/game/Projectile/ProjectileBehavior.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

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

bool stepWeapon(Armed &armed, const WeaponDefinition &weapon, bool wantsToFire)
{
    tickDown(armed.cooldown);
    const bool boosted = armed.boostTicks > 0;
    if (boosted) {
        --armed.boostTicks;
    }
    if (armed.reloadLeft > 0 && tickDown(armed.reloadLeft)) {
        armed.ammo = weapon.magazineSize;
    }
    const bool ready = armed.cooldown == 0 && armed.reloadLeft == 0;
    if (!ready || !wantsToFire) {
        return false;
    }
    armed.cooldown = boosted ? std::max<std::uint32_t>(1, armed.boostInterval) : weapon.fireInterval;
    spendRound(armed, weapon);
    return true;
}

void selectWeapon(Armed &armed, Holster &holster, std::uint8_t wanted)
{
    if (wanted == holster.inHand) {
        return;
    }
    std::swap(armed, holster.weapon);
    holster.inHand = wanted;
}

ShotLaunch aimShot(const WeaponDefinition &weapon, float projectileHalfWidth, std::uint8_t owner,
                   ecs::Position from, ecs::Position towards)
{
    float directionX = towards.x - from.x;
    float directionY = towards.y - from.y;
    const float distance = std::hypot(directionX, directionY);
    if (distance > 0.0f) {
        directionX /= distance;
        directionY /= distance;
    } else {
        directionX = owner < MaxPlayers ? 1.0f : -1.0f;
        directionY = 0.0f;
    }
    return {{from.x + (directionX * projectileHalfWidth), from.y + (directionY * projectileHalfWidth)},
            {directionX * weapon.projectileSpeed, directionY * weapon.projectileSpeed}};
}

ShotLaunch playerShot(const WeaponDefinition &weapon, float projectileHalfWidth, std::uint8_t slot,
                      ecs::Position ship, int aim, const Rules &rules)
{
    const float halfWidth = rules.shipHalfWidth;
    if (aim != 0) {
        // The ship turned by the aim angle: its nose, and its shot, that way
        const float angle = static_cast<float>(aim) * rules.aimAngle * std::numbers::pi_v<float> / 180.0f;
        const float noseX = ship.x + (std::cos(angle) * halfWidth);
        const float noseY = ship.y + (std::sin(angle) * halfWidth);
        return aimShot(weapon, projectileHalfWidth, slot, {noseX, noseY},
                       {noseX + std::cos(angle), noseY + std::sin(angle)});
    }
    const float noseX = ship.x + halfWidth;
    return aimShot(weapon, projectileHalfWidth, slot, {noseX, ship.y}, {noseX + 1.0f, ship.y});
}

ecs::Position moveShot(ecs::Position position, ProjectileCategory mechanic, const physics::Velocity &launch,
                       std::uint32_t age, const Rules &rules)
{
    const physics::Velocity velocity = getProjectileVelocity(mechanic, launch, age, rules);
    position.x += velocity.x * TickDuration;
    position.y += velocity.y * TickDuration;
    return position;
}

} // namespace rtype::game
