#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Enemy/EnemyDefinition.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/scripting/Lua.hpp"
#include "rtype/scripting/State/State.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace rtype::game
{

/// The behaviors of the enemies (EnemyDefinition::behavior), run in a sandboxed State of the game: what each
/// one sees of its enemy and of the game, and the orders it gives back (docs/architecture/game.md, "Enemy
/// behaviors"). Only the EnemySystem uses it; it is the one place of the game that binds C++ to Lua.
class EnemyScripts
{
public:
    /// Instructions a behavior may execute for one enemy at one tick: a script that loops forever is stopped
    /// at once, without slowing the game.
    static constexpr std::uint64_t InstructionLimit = 100'000;

private:
    GameContext &_context;
    scripting::State _state;
    /// The `update` function of each behavior loaded, by enemy id.
    std::unordered_map<std::string, luabridge::LuaRef> _updates;
    /// The behaviors that failed, to load or during the game: stopped until the end of the game.
    std::unordered_set<std::string> _failed;
    /// What each enemy remembers between two ticks, by entity: a new table for a new enemy.
    std::unordered_map<ecs::entity_t, luabridge::LuaRef> _memories;
    /// The game as the behaviors see it at this tick, made by startTick().
    luabridge::LuaRef _game;

    void bindTypes();
    /// The `update` function of the behavior of an enemy, loaded the first time. Nil if it failed.
    [[nodiscard]] luabridge::LuaRef findUpdate(const EnemyDefinition &definition);
    void stopBehavior(const EnemyDefinition &definition, const std::string &problem);
    [[nodiscard]] luabridge::LuaRef listPlayers(ecs::Ecs &ecs) const;
    [[nodiscard]] luabridge::LuaRef getMemory(ecs::Ecs &ecs, ecs::entity_t entity);

public:
    explicit EnemyScripts(GameContext &context);

    /// Sees the game as it is at the beginning of the tick: the players, their ships and their weapons.
    void startTick(ecs::Ecs &ecs);

    /// Runs the behavior of an enemy for this tick: it may change the velocity and the EnemyOrders of the
    /// enemy. Returns false, changing nothing, if the behavior failed, now or before: it is then logged once,
    /// and stopped for the rest of the game.
    bool runBehavior(ecs::Ecs &ecs, ecs::entity_t entity, const EnemyDefinition &definition);
};

} // namespace rtype::game
