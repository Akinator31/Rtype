#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <memory>

namespace rtype::game
{

class EnemyScripts;

/// Flies the Bydos. One whose EnemyDefinition has a behavior runs its `update(enemy, game)` at every tick,
/// which may change its velocity and its EnemyOrders, then flies to the destination of its orders, at its top
/// speed (docs/architecture/game.md, "Enemy behaviors"). One without speeds up to the left, by the
/// acceleration of its definition, up to its top speed. A behavior that fails is logged and stopped for the
/// rest of the game: its enemies lose their orders and fly as if they had none.
class EnemySystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Enemy> &_enemies;
    ecs::ComponentCache<EnemyOrders> &_orders;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Velocity> &_velocities;
    /// The scripts of the behaviors, in a Lua state of this game (no Lua in the headers of the game).
    std::unique_ptr<EnemyScripts> _scripts;

    void flyLeft(ecs::entity_t entity, const EnemyDefinition &enemy, float elapsed);
    void flyToDestination(ecs::entity_t entity, const EnemyDefinition &enemy, float elapsed);

public:
    EnemySystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);
    ~EnemySystem() override;

    EnemySystem(const EnemySystem &) = delete;
    EnemySystem &operator=(const EnemySystem &) = delete;
    EnemySystem(EnemySystem &&) = delete;
    EnemySystem &operator=(EnemySystem &&) = delete;

    void update(float elapsed) override;
};

} // namespace rtype::game
