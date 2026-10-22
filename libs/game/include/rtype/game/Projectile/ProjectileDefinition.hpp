#pragma once

#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/physics/Component/Collider.hpp"

#include <string>

namespace rtype::game
{

/// Everything that makes a kind of projectile: creating a new one is filling this and registering it in a
/// ProjectileRegistry; loadGameConfig() reads them from config/projectiles/ (ADR 0018).
struct ProjectileDefinition {
    /// Unique name in a registry, e.g. "pulse".
    std::string id;
    /// Path of the 3D model, drawn by the client. The game only carries it: it never loads it.
    std::string model;
    ProjectileCategory category = ProjectileCategory::Bullet;
    /// The shape that hits what it meets, centred on the projectile.
    physics::Collider collider;
};

} // namespace rtype::game
