#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

namespace rtype::game
{

/// Ages each projectile by one tick and sets its velocity from its category (getProjectileVelocity()). It
/// runs before the physics::MovementSystem, which moves the projectile with that velocity.
class ProjectileSystem : public ecs::System
{
    GameContext &_context;
    ecs::ComponentCache<Projectile> &_projectiles;
    ecs::ComponentCache<physics::Velocity> &_velocities;

public:
    ProjectileSystem(ecs::ComponentCaches &caches, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
