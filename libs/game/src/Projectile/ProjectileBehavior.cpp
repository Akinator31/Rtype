#include "rtype/game/Projectile/ProjectileBehavior.hpp"

#include <algorithm>

namespace rtype::game
{

physics::Velocity getProjectileVelocity(ProjectileCategory category, const physics::Velocity &launch,
                                        std::uint32_t age, const Rules &rules)
{
    switch (category) {
    case ProjectileCategory::Missile: {
        const float boost = std::min(1.0f + rules.missileBoostPerTick * static_cast<float>(age),
                                     rules.missileMaxBoost);
        return {launch.x * boost, launch.y * boost};
    }
    case ProjectileCategory::Plasma: {
        const bool up = (age / rules.plasmaZigzagInterval) % 2 == 0;
        return {launch.x, launch.y + (up ? -rules.plasmaZigzagSpeed : rules.plasmaZigzagSpeed)};
    }
    case ProjectileCategory::Bullet:
    case ProjectileCategory::Laser:
        break;
    }
    return launch;
}

const ProjectileSounds &getProjectileSounds(ProjectileCategory category)
{
    // No sound file yet: a path goes in the case of its category once the file exists
    static const ProjectileSounds none;
    switch (category) {
    case ProjectileCategory::Bullet:
    case ProjectileCategory::Missile:
    case ProjectileCategory::Laser:
    case ProjectileCategory::Plasma:
        break;
    }
    return none;
}

} // namespace rtype::game
