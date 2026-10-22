#pragma once

#include "Game/GameSession.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Simulation.hpp"

#include <cstdint>
#include <vector>

namespace rtype::client
{

/// A game offline: the Simulation of libs/game runs in the client, for one player in slot 0 (ADR 0015).
///
/// The simulation steps exactly once per tick of game time, 60 times per second, whatever the frame rate:
/// the frame times add up, and each full tick is one step, like the PhysicsStep. After a frame too long (a
/// loading, a debugger), it steps at most MaxStepsPerUpdate times and forgets the rest: the game slows down
/// for a moment instead of freezing to catch up.
class LocalSession final : public GameSession
{
public:
    /// Most steps in one update.
    static constexpr int MaxStepsPerUpdate = 5;

private:
    game::Simulation _simulation;
    game::PlayerInput _input;
    /// Game time not stepped yet, in seconds. A double: a float would drift after an hour of 1/60 s.
    double _pending = 0.0;
    bool _paused = false;
    bool _over = false;
    std::vector<game::Snapshot> _snapshots;

public:
    /// @param config The definitions of the game, loaded once by the client (loadGameConfig()).
    explicit LocalSession(game::Rules rules = {}, const game::GameConfig &config = {});

    [[nodiscard]] std::uint8_t localSlot() const override { return 0; }
    void setInput(game::PlayerInput input) override { _input = input; }
    void update(float elapsed) override;
    [[nodiscard]] std::vector<game::Snapshot> takeSnapshots() override;
    void setPaused(bool paused) override { _paused = paused; }
    [[nodiscard]] bool isOver() const override { return _over; }

    /// The simulation, for the tests.
    [[nodiscard]] const game::Simulation &simulation() const { return _simulation; }
};

} // namespace rtype::client
