#pragma once

#include "rtype/game/Constants.hpp"
#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Player/PlayerDefinition.hpp"
#include "rtype/game/PlayerInput.hpp"
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
    /// The tick being simulated, from 1.
    std::uint32_t tick = 0;
    std::array<PlayerSlot, MaxPlayers> players{};
    WaveProgress wave;
    /// The game ended, won or lost: nothing more happens to the waves nor to the outcome.
    bool over = false;

    /// Events since the previous snapshot.
    std::vector<DestroyedEntity> destroyed;
    std::optional<std::uint16_t> waveStarted;
    std::optional<GameOutcome> gameOver;

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
