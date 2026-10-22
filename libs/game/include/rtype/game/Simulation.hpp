#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/GameContext.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstddef>
#include <cstdint>

namespace rtype::game
{

/// One game of R-Type: its own ECS, the systems of the rules, and the players. It knows neither the network
/// nor the drawing: it takes the inputs of the players, steps one tick at a time, and gives a snapshot of the
/// world (ADR 0015). The instances of the server and the client offline use the same class.
///
/// @code
/// rtype::game::Simulation simulation;
/// simulation.addPlayer(0);
///
/// // Every tick (TickRate per second)
/// simulation.setInput(0, input);
/// simulation.step();
/// const rtype::game::Snapshot &world = simulation.snapshot();
/// @endcode
///
/// It decides nothing about time: whoever uses it calls step() once per tick (a Timer on the server, an
/// accumulator of the frame times on the client, a loop in the tests). The same inputs, in the same order,
/// give the same snapshots: a game can be replayed, and the tests are exact.
///
/// Not thread-safe: use it from one thread, the simulation thread.
class Simulation
{
    GameContext _context;
    // After the context, which its systems use: destroyed before it
    ecs::Ecs _ecs;
    Snapshot _snapshot;

    void buildSnapshot();
    void registerConfig(const GameConfig &config);

public:
    /// @param config The definitions of the game (loadGameConfig()), registered in the context.
    /// @throws std::invalid_argument if a definition is refused by its registry: a config that
    /// loadGameConfig() accepted never is.
    explicit Simulation(Rules rules = {}, const GameConfig &config = {});

    Simulation(const Simulation &) = delete;
    Simulation &operator=(const Simulation &) = delete;
    Simulation(Simulation &&) = delete;
    Simulation &operator=(Simulation &&) = delete;

    /// Adds a player in a slot, with its ship.
    /// @throws std::invalid_argument if the slot is above 3 or already taken.
    void addPlayer(std::uint8_t slot);

    /// Removes a player and its ship (DestroyCause::Removed in the next snapshot). Does nothing for an empty
    /// slot.
    void removePlayer(std::uint8_t slot);

    [[nodiscard]] bool hasPlayer(std::uint8_t slot) const;
    [[nodiscard]] std::size_t playerCount() const;

    /// The input of a player, applied at every tick until the next call. Ignored for an empty slot.
    void setInput(std::uint8_t slot, PlayerInput input);

    /// Simulates exactly one tick (TickDuration), then takes the snapshot.
    void step();

    /// The world at the end of the last step(): what changed since (a player added or removed) shows in the
    /// next one.
    [[nodiscard]] const Snapshot &snapshot() const { return _snapshot; }

    /// The number of the last tick simulated, 0 before the first step().
    [[nodiscard]] std::uint32_t tick() const { return _context.tick; }

    [[nodiscard]] const Rules &rules() const { return _context.rules; }

    /// The ECS of the game, for the tests and the debugging tools.
    [[nodiscard]] ecs::Ecs &ecs() { return _ecs; }

    /// What the systems share, for the tests.
    [[nodiscard]] GameContext &context() { return _context; }
};

} // namespace rtype::game
