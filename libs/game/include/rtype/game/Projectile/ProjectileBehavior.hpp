#pragma once

#include "rtype/game/Projectile/ProjectileCategory.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cstdint>
#include <string>

/// What each ProjectileCategory does. A C++ enum has no methods: the behavior of a category is a function
/// that takes it, one `switch` per question, so that adding a category means adding a case in each.
namespace rtype::game
{

/// The sounds of a category, as paths (empty: no sound). The game only carries them: it has no audio, the
/// client plays them. There is no sound file yet, so every path is empty.
struct ProjectileSounds {
    /// When the projectile appears.
    std::string spawn;
    /// While it flies.
    std::string move;
    /// When it goes without hitting anything (it left the game area).
    std::string vanish;
    /// When it hits something.
    std::string impact;
};

/// The velocity of a projectile of this category `age` ticks after its launch, from the velocity it was
/// launched with:
/// - Bullet and Laser keep it;
/// - Missile speeds up along it by Rules::missileBoostPerTick per tick, up to Rules::missileMaxBoost times;
/// - Plasma zigzags: it adds Rules::plasmaZigzagSpeed up then down, every Rules::plasmaZigzagInterval ticks.
[[nodiscard]] physics::Velocity getProjectileVelocity(ProjectileCategory category,
                                                      const physics::Velocity &launch, std::uint32_t age,
                                                      const Rules &rules);

[[nodiscard]] const ProjectileSounds &getProjectileSounds(ProjectileCategory category);

} // namespace rtype::game
