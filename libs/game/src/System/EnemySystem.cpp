#include "rtype/game/System/EnemySystem.hpp"

#include "rtype/game/Entities.hpp"

namespace rtype::game
{

EnemySystem::EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _enemies(use<Enemy>(caches)), _positions(use<ecs::Position>(caches)),
      _spawnCooldown(context.rules.enemySpawnInterval)
{
}

void EnemySystem::update(float)
{
    std::size_t alive = 0;
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (!_enemies.has(entity) || !_positions.has(entity) || !_ecs.isAlive(entity)) {
            continue;
        }
        ++alive;
        shootWhenReady(entity);
    }
    spawnWhenDue(alive);
}

void EnemySystem::shootWhenReady(ecs::entity_t entity)
{
    const Rules &rules = _context.rules;
    Enemy &enemy = _enemies.get(entity);
    if (enemy.fireCooldown > 0) {
        --enemy.fireCooldown;
    }
    if (enemy.fireCooldown > 0) {
        return;
    }
    enemy.fireCooldown = _context.randomBetween(rules.enemyMinFireInterval, rules.enemyMaxFireInterval);
    // From the nose of the Bydo. Created after the systems: it would grow the caches being read
    const ecs::Position position = _positions.get(entity);
    const float x = position.x - rules.enemyHalfWidth - rules.enemyShotHalfWidth;
    _ecs.defer([this, x, y = position.y] { spawnEnemyShot(_ecs, _context, x, y); });
}

void EnemySystem::spawnWhenDue(std::size_t alive)
{
    if (alive >= _context.rules.maxEnemies) {
        return;
    }
    if (_spawnCooldown > 0) {
        --_spawnCooldown;
    }
    if (_spawnCooldown == 0) {
        _spawnCooldown = _context.rules.enemySpawnInterval;
        _ecs.defer([this] { spawnEnemy(_ecs, _context); });
    }
}

} // namespace rtype::game
