#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/physics/Component/Motion.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace rtype::game
{

/// Runs the rescue against the final boss, offline only (Rules::rescue, context.rescue). The CombatSystem
/// starts it instead of the defeat when the last ship of the last player is lost against a boss in the last
/// wave; from then on, the RescueSystem flies the boss, the allies and the ship of the player, stage by stage
/// (RescueStage):
///
/// 1. **Voice**, Rules::rescueVoiceTicks: the client plays the voice; the boss guards on its spot.
/// 2. **Arrival**, Rules::rescueArrivalTicks: five allies fly in from the left with a new ship for the
///    player, who gets a life back, into an arrowhead formation, the player at its tip.
/// 3. **Assault**, Rules::rescueAssaultTicks: the allies shoot at the boss, which guards and cannot be hurt;
///    the player flies and fires freely.
/// 4. **Dash**: the boss raises its blade, then dashes through every ship of the formation in turn, throwing
///    each one back without hurting it (context.struck), and goes back to its spot.
/// 5. **Prompt**: the client asks for Fire (Space); a Fire held since before must be released first.
/// 6. **Combined**, Rules::rescueCombinedTicks: every ship fires at the boss at once, its health drains to 0,
///    then it is killed (the player scores it).
/// 7. **Finished**: the victory, Rules::rescueVictoryDelay ticks later.
///
/// While the rescue runs, the waves and the behaviors of the Bydos wait, and nothing hurts the ships of the
/// players (WaveSystem, EnemySystem, CombatSystem). The new entities are created after the systems.
class RescueSystem : public ecs::System
{
public:
    /// Ships of the formation: the player and its allies.
    static constexpr std::size_t FormationSize = 6;

private:
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Enemy> &_enemies;
    ecs::ComponentCache<EnemyOrders> &_orders;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<physics::Velocity> &_velocities;
    ecs::ComponentCache<Health> &_healths;
    ecs::ComponentCache<Ally> &_allies;
    ecs::ComponentCache<NetworkId> &_ids;

    void enter(RescueStage stage);
    [[nodiscard]] std::optional<ecs::entity_t> findBoss() const;
    /// The slot of the player rescued: the first one in the game.
    [[nodiscard]] std::optional<std::uint8_t> findRescuedSlot() const;
    /// A ship of the formation: 0 is the ship of the player, 1 to 5 the allies.
    [[nodiscard]] std::optional<ecs::entity_t> findShip(std::size_t index) const;

    /// The boss takes a stance for this tick, fires nothing, and cannot be hurt.
    void order(ecs::entity_t boss, EnemyStance stance);
    /// Sets the velocity of an entity towards a point, at most `speed`, to stop on it.
    void steer(ecs::entity_t entity, float x, float y, float speed, float elapsed);
    /// Keeps the ships in formation, or lets those thrown back by a dash drift. The ship of the player is
    /// left to its input when `playerFree`.
    void holdFormation(float elapsed, bool playerFree);
    /// The ships whose turn it is fire at the boss: every `interval` ticks each, the player's ship too if
    /// `withPlayer`.
    void fireAt(ecs::entity_t boss, std::uint32_t interval, bool withPlayer);

    void callAllies();
    void runDash(ecs::entity_t boss, float elapsed);
    void runPrompt();
    void runCombined(ecs::entity_t boss);

public:
    RescueSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
