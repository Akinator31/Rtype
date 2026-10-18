#pragma once

#include "rtype/game/Constants.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Snapshot.hpp"

#include <array>
#include <cstdint>
#include <optional>
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
};

/// What the systems of the game share besides the components: the rules, the tick, the players and their
/// inputs, the ids, and the events of the tick that go to the next snapshot. Owned by the Simulation, given
/// to each system when it is made.
struct GameContext {
    Rules rules;
    /// The tick being simulated, from 1.
    std::uint32_t tick = 0;
    std::array<PlayerSlot, MaxPlayers> players{};

    /// Events since the previous snapshot.
    std::vector<DestroyedEntity> destroyed;
    std::optional<std::uint16_t> waveStarted;
    std::optional<GameOutcome> gameOver;

    /// A new NetworkId: 1, 2, 3... never twice in a game.
    [[nodiscard]] std::uint32_t newId() { return _nextId++; }

private:
    std::uint32_t _nextId = 1;
};

} // namespace rtype::game
