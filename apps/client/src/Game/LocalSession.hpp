#pragma once

#include "Game/GameSession.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Simulation.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// A game offline: the Simulation of libs/game runs in the client, for one player in slot 0 (ADR 0015). It is
/// the story mode: the rules always have Rules::rescue, so that allies come to the rescue against the final
/// boss when its last ship is lost.
///
/// The simulation steps exactly once per tick of game time, 60 times per second, whatever the frame rate:
/// the frame times add up, and each full tick is one step, like the PhysicsStep. After a frame too long (a
/// loading, a debugger), it steps at most MaxStepsPerUpdate times and forgets the rest: the game slows down
/// for a moment instead of freezing to catch up.
///
/// Between two steps, the screen draws more frames (144 per second): between() gives the world on the way to
/// the next tick, by the fraction of the time to it. The ship of the player moves with the input held, as the
/// next step will move it (game::moveShip()), so that a key pressed shows at once; the rest goes on with its
/// velocity.
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
    /// The world at the last step, for between().
    std::optional<game::Snapshot> _last;
    /// The waves of the game config, played one after the other.
    std::uint16_t _waveCount = 0;
    /// The weapons of the player, empty with the one of the game alone.
    game::Loadout _loadout;

public:
    /// @param config The definitions of the game, loaded once by the client (loadGameConfig()).
    /// @param loadout The weapons the player chose in the hangar; empty, that of the config alone.
    /// @param ship The code of the ship the player equipped in the hangar, whose characteristics the game
    /// applies (Simulation::setShip()); empty, or not in the hangar of the config, the ship of the rules.
    explicit LocalSession(game::Rules rules = {}, const game::GameConfig &config = {},
                          const game::Loadout &loadout = {}, const std::string &ship = {});

    [[nodiscard]] std::uint8_t localSlot() const override { return 0; }
    [[nodiscard]] std::optional<std::uint16_t> waveCount() const override { return _waveCount; }
    void setInput(game::PlayerInput input) override { _input = input; }
    [[nodiscard]] game::Loadout loadout() const override { return _loadout; }
    void update(float elapsed) override;
    [[nodiscard]] std::vector<game::Snapshot> takeSnapshots() override;
    [[nodiscard]] std::optional<game::Snapshot> between() const override;
    void setPaused(bool paused) override { _paused = paused; }
    [[nodiscard]] bool isOver() const override { return _over; }
    /// None: the game runs in the client.
    [[nodiscard]] std::optional<std::chrono::milliseconds> ping() const override { return std::nullopt; }
    /// The collider of the entity in the simulation: its real hitbox.
    [[nodiscard]] std::optional<physics::Collider> findCollider(const game::EntityState &state) override;

    /// The simulation, for the tests.
    [[nodiscard]] const game::Simulation &simulation() const { return _simulation; }
};

} // namespace rtype::client
