#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Collider.hpp"

#include <optional>
#include <unordered_set>
#include <vector>

namespace rtype::game
{

/// The hits of the game. A Bydo shot hurts the ships: a ship that is hit is destroyed, its player loses a
/// life, and gets a new ship after Rules::respawnDelay ticks, invulnerable for Rules::invulnerableTicks,
/// until it has no life left. Once every player is out of lives, the game is over
/// (GameOutcome::Defeat). The entities are destroyed after the systems of the tick.
class CombatSystem : public ecs::System
{
    struct Targets {
        std::vector<ecs::entity_t> dangers;
        std::vector<ecs::entity_t> ships;
    };

    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Kind> &_kinds;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Collider> &_colliders;
    ecs::ComponentCache<Health> &_healths;
    ecs::ComponentCache<Invulnerable> &_invulnerables;
    ecs::ComponentCache<PlayerShip> &_playerShips;
    bool _over = false;

    [[nodiscard]] Targets gatherTargets();
    [[nodiscard]] bool overlap(ecs::entity_t first, ecs::entity_t second) const;
    void hitShips(const Targets &targets, std::unordered_set<ecs::entity_t> &spent);
    [[nodiscard]] std::optional<ecs::entity_t>
    findDanger(ecs::entity_t ship, const Targets &targets,
               const std::unordered_set<ecs::entity_t> &spent) const;
    void loseShip(ecs::entity_t ship);
    void destroyLater(ecs::entity_t entity, DestroyCause cause);
    void tickInvulnerability(const Targets &targets);
    void respawnShips();
    void endGameWhenDefeated();

public:
    CombatSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
