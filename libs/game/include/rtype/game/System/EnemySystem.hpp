#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"

namespace rtype::game
{

/// Makes a Bydo appear every Rules::enemySpawnInterval ticks while there are fewer than Rules::maxEnemies,
/// and makes each one shoot after a random number of ticks, between Rules::enemyMinFireInterval and
/// Rules::enemyMaxFireInterval. The new entities are created after the systems of the tick, so they move from
/// the next one.
class EnemySystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Enemy> &_enemies;
    ecs::ComponentCache<ecs::Position> &_positions;
    std::uint32_t _spawnCooldown;

    void shootWhenReady(ecs::entity_t entity);
    void spawnWhenDue(std::size_t alive);

public:
    EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
