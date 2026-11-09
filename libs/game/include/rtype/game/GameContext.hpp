#pragma once

#include "rtype/game/Constants.hpp"
#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Player/PlayerDefinition.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Progression.hpp"
#include "rtype/game/Projectile/ProjectileRegistry.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Ship/ShipRegistry.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/game/Wave/WaveProgress.hpp"
#include "rtype/game/Wave/WaveRegistry.hpp"
#include "rtype/game/Weapon/WeaponRegistry.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace rtype::game
{

/// A player of the game, in its slot.
struct PlayerSlot {
    bool present = false;
    /// The input applied at each tick, until the next setInput().
    PlayerInput input;
    std::uint8_t lives = 0;
    std::uint32_t score = 0;
    /// Ticks left before its next ship appears, 0 when it has one or no lives left.
    std::uint32_t respawnIn = 0;
    /// Bydos it shot down.
    std::uint32_t kills = 0;
    /// Waves that ended while its ship was alive, and how many of them in a row up to the last one.
    std::uint16_t wavesCleared = 0;
    std::uint16_t streak = 0;
    std::uint16_t bestStreak = 0;
    /// Experience earned in this game, endless games only (Progression).
    std::uint32_t xp = 0;

    /// Its ship is in the game: it has lives left and is not waiting for the next one.
    [[nodiscard]] bool isAlive() const { return present && lives > 0 && respawnIn == 0; }
};

/// Where the rescue against the final boss is, run by the RescueSystem (Rules::rescue).
struct RescueProgress {
    RescueStage stage = RescueStage::None;
    /// Ticks since the stage started.
    std::uint32_t ticks = 0;
    /// Dash stage: the ship the shogun dashes at, by index in the formation (0 is the player), and whether
    /// it is flying at it or raising its blade before.
    std::size_t target = 0;
    bool dashing = false;
    /// Dash stage: where the current dash ends.
    float dashX = 0.0f;
    float dashY = 0.0f;
    /// Ticks the ship of the player is still thrown back by a dash.
    std::uint32_t playerStunned = 0;
    /// Prompt stage: Fire was released since the prompt showed, so that a Fire held from before does not
    /// count.
    bool fireReleased = false;
    /// Combined stage: the health of the shogun when it started, drained to 0.
    std::uint8_t bossHealth = 0;
};

/// What the systems of the game share besides the components: the rules, the tick, the players and their
/// inputs, the ids, and the events of the tick that go to the next snapshot. Owned by the Simulation, given
/// to each system when it is made.
struct GameContext {
    Rules rules;
    ProjectileRegistry projectiles;
    ShipRegistry ships;
    WeaponRegistry weapons;
    EnemyRegistry enemies;
    WaveRegistry waves;
    /// The ship and the weapon of every player.
    PlayerDefinition player;
    /// The experience of the endless games.
    Progression progression;
    /// The tick being simulated, from 1.
    std::uint32_t tick = 0;
    std::array<PlayerSlot, MaxPlayers> players{};
    WaveProgress wave;
    RescueProgress rescue;
    /// The game ended, won or lost: nothing more happens to the waves nor to the outcome.
    bool over = false;

    /// Events since the previous snapshot.
    std::vector<DestroyedEntity> destroyed;
    std::optional<std::uint16_t> waveStarted;
    /// The music of the wave started, empty if it has none: set with waveStarted.
    std::string waveMusic;
    std::optional<GameOutcome> gameOver;
    /// The ships the blade of the shogun went through during the rescue, by NetworkId.
    std::vector<std::uint32_t> struck;

    /// Counts a Bydo shot down by a player, and its experience in an endless game.
    void rewardKill(PlayerSlot &killer, const EnemyDefinition &enemy)
    {
        killer.score += enemy.score;
        ++killer.kills;
        if (rules.endless) {
            killer.xp += applyMultiplier(enemy.xp, streakMultiplier(progression, killer.streak));
        }
    }

    /// A number from `first` to `last` included. Same result on every platform, unlike the distributions of
    /// the standard library, so that a game replays the same everywhere.
    [[nodiscard]] std::uint32_t randomBetween(std::uint32_t first, std::uint32_t last)
    {
        return first + static_cast<std::uint32_t>(_random() % (last - first + 1));
    }

    /// A number from `min` to `max` included, in steps of a thousandth of the interval, as reproducible as
    /// randomBetween().
    [[nodiscard]] float randomNumber(float min, float max)
    {
        constexpr std::uint32_t Steps = 1000;
        return min + (max - min) * static_cast<float>(randomBetween(0, Steps)) / static_cast<float>(Steps);
    }

    /// A new NetworkId: 1, 2, 3... never twice in a game.
    [[nodiscard]] std::uint32_t newId() { return _nextId++; }

private:
    std::uint32_t _nextId = 1;
    std::mt19937 _random{1};

public:
    /// Restarts the random numbers from the seed of the rules.
    void seedRandom() { _random.seed(rules.seed); }
};

} // namespace rtype::game
