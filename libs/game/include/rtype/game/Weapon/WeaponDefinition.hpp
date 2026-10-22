#pragma once

#include "rtype/game/Projectile/ProjectileCategory.hpp"

#include <cstdint>
#include <string>

namespace rtype::game
{

/// WeaponDefinition::magazineSize of a weapon that never runs out of ammunition.
inline constexpr std::uint32_t InfiniteAmmo = 0;

/// Everything that makes a kind of weapon: creating a new one is filling this and registering it in a
/// WeaponRegistry; loadGameConfig() reads them from config/weapons/ (ADR 0018). Durations are in ticks
/// (TickRate per second), distances in game units.
struct WeaponDefinition {
    /// Unique name in a registry, e.g. "blaster".
    std::string id;
    /// The ammunition: the id of a ProjectileDefinition of the ProjectileRegistry.
    std::string projectile;
    /// The firing mechanic: how the projectiles it fires move (physics). It replaces the category of the
    /// projectile definition, so that one kind of projectile can be fired in different ways.
    ProjectileCategory mechanic = ProjectileCategory::Bullet;
    /// Speed the projectiles leave with, to the right.
    float projectileSpeed = 1500.0f;
    /// Ticks between two shots while Fire is held: 10 is 6 shots per second.
    std::uint32_t fireInterval = 10;
    /// Distance a projectile flies before it vanishes, 0 for no limit (it flies until it leaves the area).
    float range = 0.0f;
    /// Shots before the weapon must be reloaded, InfiniteAmmo for a weapon that never runs out (it never
    /// reloads, reloadTicks is ignored).
    std::uint32_t magazineSize = 0;
    /// Ticks to reload an empty magazine. The weapon reloads on its own and cannot fire meanwhile.
    std::uint32_t reloadTicks = 0;
};

} // namespace rtype::game
