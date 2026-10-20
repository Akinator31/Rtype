#include "rtype/game/System/ProjectileSystem.hpp"

#include "rtype/game/Projectile/ProjectileBehavior.hpp"

namespace rtype::game
{

ProjectileSystem::ProjectileSystem(ecs::ComponentCaches &caches, GameContext &context)
    : _context(context), _projectiles(use<Projectile>(caches)), _velocities(use<physics::Velocity>(caches))
{
}

void ProjectileSystem::update(float)
{
    for (ecs::entity_t entity = 0; entity < _projectiles.size(); ++entity) {
        if (!_projectiles.has(entity) || !_velocities.has(entity)) {
            continue;
        }
        Projectile &projectile = _projectiles.get(entity);
        _velocities.get(entity) =
            getProjectileVelocity(projectile.category, projectile.launch, projectile.age, _context.rules);
        ++projectile.age;
    }
}

} // namespace rtype::game
