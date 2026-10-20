#include "rtype/game/System/ProjectileSystem.hpp"

#include "rtype/game/Entities.hpp"
#include "rtype/game/Projectile/ProjectileBehavior.hpp"

#include <cmath>

namespace rtype::game
{

ProjectileSystem::ProjectileSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _projectiles(use<Projectile>(caches)), _velocities(use<physics::Velocity>(caches))
{
}

void ProjectileSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _projectiles.size(); ++entity) {
        if (!_projectiles.has(entity) || !_velocities.has(entity)) {
            continue;
        }
        Projectile &projectile = _projectiles.get(entity);
        physics::Velocity &velocity = _velocities.get(entity);
        velocity =
            getProjectileVelocity(projectile.category, projectile.launch, projectile.age, _context.rules);
        ++projectile.age;

        projectile.travelled += std::hypot(velocity.x, velocity.y) * elapsed;
        if (projectile.range > 0.0f && projectile.travelled >= projectile.range) {
            _ecs.defer([this, entity] { destroyEntity(_ecs, _context, entity, DestroyCause::Removed); });
        }
    }
}

} // namespace rtype::game
