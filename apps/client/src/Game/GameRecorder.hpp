#pragma once

#include "Game/GameSessionSystem.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>

namespace rtype::client
{

/// Events of the engine triggered by the games, for whatever listens to them: the quests. They carry nothing:
/// what a listener needs to know is counted in the stats below.
inline constexpr const char *GameStartedEvent = "game:started";
/// A Bydo shot down by the local player.
inline constexpr const char *BydoKilledEvent = "game:bydo_killed";
/// A boss shot down, by anyone.
inline constexpr const char *BossKilledEvent = "game:boss_killed";
/// The ship of the local player shot down.
inline constexpr const char *PlayerKilledEvent = "game:player_killed";
inline constexpr const char *WaveStartedEvent = "game:wave_started";
inline constexpr const char *GameWonEvent = "game:won";
inline constexpr const char *GameLostEvent = "game:lost";
/// The player left the game, or its session is over.
inline constexpr const char *GameEndedEvent = "game:ended";

/// Stats of the player, kept in the settings from one run to the next, as integers.
inline constexpr const char *GamesPlayedStat = "stats.games_played";
inline constexpr const char *BydosKilledStat = "stats.bydos_killed";
inline constexpr const char *FlyersKilledStat = "stats.flyers_killed";
inline constexpr const char *ShootersKilledStat = "stats.shooters_killed";
inline constexpr const char *TanksKilledStat = "stats.tanks_killed";
inline constexpr const char *BossesDefeatedStat = "stats.bosses_defeated";
inline constexpr const char *DeathsStat = "stats.deaths";
/// The highest wave reached.
inline constexpr const char *BestWaveStat = "stats.best_wave";
inline constexpr const char *VictoriesStat = "stats.victories";
inline constexpr const char *DefeatsStat = "stats.defeats";
/// The highest score of the local player at the end of a game.
inline constexpr const char *BestScoreStat = "stats.best_score";
/// The scores of every game, added.
inline constexpr const char *TotalScoreStat = "stats.total_score";

/// A stat of the player: 0 until it is counted.
[[nodiscard]] int getStat(const engine::Settings &settings, const char *stat);

/// Tells the engine what happens in the games: each time, it counts it in the stats, in the settings, then
/// triggers its event. The stats are counted at once, so that a listener woken once by several triggers
/// reads them all. The settings are written to their file at the end of each game.
class GameRecorder
{
    engine::Engine &_engine;

    void addToStat(const char *stat, int amount);
    void raiseStat(const char *stat, int value);

public:
    /// Creates the events, so that they can be listened to before any game. The engine must outlive it.
    explicit GameRecorder(engine::Engine &engine);

    void recordStart();
    /// Counts a ship killed: a Bydo shot down by the local player, a boss shot down, the ship of the local
    /// player.
    void recordKill(const Kill &kill, std::uint8_t localSlot);
    void recordWave(std::uint16_t wave);
    void recordOutcome(game::GameOutcome outcome);
    /// Counts the score of the local player, and writes the settings.
    void recordEnd(std::uint32_t score);
};

} // namespace rtype::client
