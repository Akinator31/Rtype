#pragma once

#include <cstdint>

namespace rtype::game
{

/// What a projectile is, whatever its model: the rules that depend on its behavior (and, later, its sounds)
/// are written per category.
enum class ProjectileCategory : std::uint8_t {
    Bullet,
    Missile,
    Laser,
    Plasma,
};

} // namespace rtype::game
