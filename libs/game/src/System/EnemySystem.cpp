#include "rtype/game/System/EnemySystem.hpp"

#include <algorithm>

namespace rtype::game
{

EnemySystem::EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _enemies(use<Enemy>(caches)), _velocities(use<physics::Velocity>(caches))
{
}

void EnemySystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (!_enemies.has(entity) || !_velocities.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        const EnemyDefinition &enemy = _context.enemies.getDefinition(_enemies.get(entity).id);
        const float gain = enemy.acceleration > 0.0f ? enemy.acceleration * elapsed : enemy.speed;

        physics::Velocity &velocity = _velocities.get(entity);
        velocity.x = std::max(velocity.x - gain, -enemy.speed);
    }
}

} // namespace rtype::game
