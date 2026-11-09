#pragma once

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Weapon/WeaponDefinition.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cstdint>

/// How a weapon fires, in one place: the game uses it in its systems (WeaponSystem, ProjectileSystem), and
/// the client to predict the shots of its player online (docs/architecture/client-online.md). Both must give
/// the same ticks and the same positions, to the bit, or every predicted shot would be corrected.
namespace rtype::game
{

/// Where a shot starts, and the velocity it is launched with.
struct ShotLaunch {
    ecs::Position position;
    physics::Velocity velocity;
};

/// One tick of a weapon: counts down its cooldown and its reload, then fires if it is ready and asked to,
/// spending a round and starting to reload on the last one.
/// @return Whether it fires this tick.
[[nodiscard]] bool stepWeapon(Armed &armed, const WeaponDefinition &weapon, bool wantsToFire);

/// A shot fired from a point towards another: it starts half its width further along the line, at the speed
/// of the weapon. Towards the point itself, it flies forward: right for a player, left for the others.
/// @param projectileHalfWidth Half the width of the collider of the projectile of the weapon.
[[nodiscard]] ShotLaunch aimShot(const WeaponDefinition &weapon, float projectileHalfWidth,
                                 std::uint8_t owner, ecs::Position from, ecs::Position towards);

/// The shot of the ship of a player: from its nose, forward, or turned up (`aim` −1) or down (+1) by
/// Rules::aimAngle.
/// @param ship The position of the ship when it fires: before it moves in the tick.
[[nodiscard]] ShotLaunch playerShot(const WeaponDefinition &weapon, float projectileHalfWidth,
                                    std::uint8_t slot, ecs::Position ship, int aim, const Rules &rules);

/// The position of a shot after one tick, at `age` ticks after its launch: moved for TickDuration by the
/// velocity of its mechanic (getProjectileVelocity()), as ProjectileSystem and physics::MovementSystem do.
[[nodiscard]] ecs::Position moveShot(ecs::Position position, ProjectileCategory mechanic,
                                     const physics::Velocity &launch, std::uint32_t age, const Rules &rules);

} // namespace rtype::game
