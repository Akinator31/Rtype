#include "rtype/game/System/EnemySystem.hpp"

#include "Enemy/EnemyScripts.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::game
{

EnemySystem::EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _enemies(use<Enemy>(caches)), _orders(use<EnemyOrders>(caches)),
      _positions(use<ecs::Position>(caches)), _velocities(use<physics::Velocity>(caches)),
      _scripts(std::make_unique<EnemyScripts>(context))
{
}

EnemySystem::~EnemySystem() = default;

void EnemySystem::update(float elapsed)
{
    _scripts->startTick(_ecs);
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (!_enemies.has(entity) || !_velocities.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        Enemy &enemy = _enemies.get(entity);
        const EnemyDefinition &definition = _context.enemies.getDefinition(enemy.id);
        if (!_orders.has(entity)) {
            flyLeft(entity, definition, elapsed);
        } else if (_scripts->runBehavior(_ecs, entity, definition)) {
            flyToDestination(entity, definition, elapsed);
        } else {
            _ecs.removeComponent<EnemyOrders>(entity);
            flyLeft(entity, definition, elapsed);
        }
        ++enemy.age;
    }
}

void EnemySystem::flyLeft(ecs::entity_t entity, const EnemyDefinition &enemy, float elapsed)
{
    const float gain = enemy.acceleration > 0.0f ? enemy.acceleration * elapsed : enemy.speed;
    physics::Velocity &velocity = _velocities.get(entity);
    velocity.x = std::max(velocity.x - gain, -enemy.speed);
}

void EnemySystem::flyToDestination(ecs::entity_t entity, const EnemyDefinition &enemy, float elapsed)
{
    const EnemyOrders &orders = _orders.get(entity);
    if (!orders.destination || !_positions.has(entity) || elapsed <= 0.0f) {
        return;
    }
    const ecs::Position &position = _positions.get(entity);
    const float distanceX = orders.destination->x - position.x;
    const float distanceY = orders.destination->y - position.y;
    const float distance = std::hypot(distanceX, distanceY);
    physics::Velocity &velocity = _velocities.get(entity);
    if (distance == 0.0f) {
        velocity = {};
        return;
    }
    // Slower on the last tick, so that it stops on the destination instead of flying past it
    const float speed = std::min(enemy.speed, distance / elapsed);
    velocity = {distanceX / distance * speed, distanceY / distance * speed};
}

} // namespace rtype::game
