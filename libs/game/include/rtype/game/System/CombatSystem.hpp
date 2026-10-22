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

/// The hits of the game: what has Damage takes that many hit points from what it touches.
///
/// - A shot of a player hurts a Bydo, and is spent. A Bydo with no hit points left is destroyed, and its
///   EnemyDefinition::score goes to the player of the shot.
/// - A Bydo shot hurts a ship, and is spent; a Bydo hurts a ship it runs into, and keeps going. A ship that
///   survives cannot be hit for Rules::hitInvulnerableTicks. One with no hit points left is destroyed, its
///   player loses a life, and gets a new ship after Rules::respawnDelay ticks, invulnerable for
///   Rules::invulnerableTicks, until it has no life left. Once every player is out of lives, the game is over
///   (GameOutcome::Defeat).
///
/// The entities are destroyed after the systems of the tick.
class CombatSystem : public ecs::System
{
    struct Targets {
        /// The shots of the players: they hurt the Bydos.
        std::vector<ecs::entity_t> playerShots;
        /// The Bydos and their shots: they hurt the ships.
        std::vector<ecs::entity_t> dangers;
        std::vector<ecs::entity_t> enemies;
        std::vector<ecs::entity_t> ships;
    };

    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Owner> &_owners;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Collider> &_colliders;
    ecs::ComponentCache<Health> &_healths;
    ecs::ComponentCache<Damage> &_damages;
    ecs::ComponentCache<Invulnerable> &_invulnerables;
    ecs::ComponentCache<PlayerShip> &_playerShips;
    ecs::ComponentCache<Enemy> &_enemies;

    [[nodiscard]] Targets gatherTargets();
    void sortTarget(ecs::entity_t entity, Targets &targets) const;
    [[nodiscard]] bool overlap(ecs::entity_t first, ecs::entity_t second) const;
    [[nodiscard]] std::optional<ecs::entity_t>
    findTouching(ecs::entity_t entity, const std::vector<ecs::entity_t> &candidates,
                 const std::unordered_set<ecs::entity_t> &spent) const;
    void hitEnemies(const Targets &targets, std::unordered_set<ecs::entity_t> &spent);
    void hitShips(const Targets &targets, std::unordered_set<ecs::entity_t> &spent);
    /// Takes the damage of `source` from the health of `target`. Returns true if it has none left.
    bool hurt(ecs::entity_t target, ecs::entity_t source);
    void rewardKill(ecs::entity_t shot, ecs::entity_t enemy);
    void protectShip(ecs::entity_t ship);
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
