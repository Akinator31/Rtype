#pragma once

#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Snapshot.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>

namespace rtype::server
{

/// What the game thread gives the network thread after each tick.
struct TickResult {
    rtype::game::Snapshot snapshot;
    /// For each slot, the tick the client stamped on the last input applied, 0 if none: the "last input" of
    /// the snapshots sent to it.
    std::array<std::uint32_t, rtype::game::MaxPlayers> lastInputs{};
};

/// The simulation thread of an instance (ADR 0013): it owns the Simulation and steps it at a fixed rate, 60
/// ticks per second, whatever the network does. The network thread hands it commands (players, inputs), and
/// it hands back the result of each tick: they share nothing else.
///
/// @code
/// rtype::server::GameThread game({}, config, [&](rtype::server::TickResult result) {
///     loop.post([&, result = std::move(result)] { host.publish(result); }); // to the network thread
/// });
/// game.start();
/// game.addPlayer(0);                     // from the network thread
/// game.receiveInputs(0, message.tick, message.inputs);
/// @endcode
///
/// The commands are applied at the start of the next tick, in their order. Each input is kept until the tick
/// its client stamped it with, then applied (InputBuffer): a tick without one repeats the last.
class GameThread
{
public:
    /// Called on the game thread after each tick: hand the result to the network thread.
    using PublishFunction = std::function<void(TickResult result)>;
    /// Called on the game thread if the simulation throws: the game cannot go on.
    using FailureFunction = std::function<void()>;

    /// The duration of a tick, to the nanosecond.
    static constexpr std::chrono::nanoseconds TickInterval =
        std::chrono::nanoseconds(std::chrono::seconds(1)) / rtype::game::TickRate;
    /// Behind by more than this, the thread stops catching up and starts again from now: a machine that
    /// slept, or a debugger, does not make the game run fast afterwards.
    static constexpr std::chrono::milliseconds MaxLag{250};

private:
    struct AddPlayer {
        std::uint8_t slot;
    };
    struct RemovePlayer {
        std::uint8_t slot;
    };
    struct ReceiveInputs {
        std::uint8_t slot;
        std::uint32_t tick;
        std::vector<rtype::game::PlayerInput> inputs;
    };
    using Command = std::variant<AddPlayer, RemovePlayer, ReceiveInputs>;

    rtype::game::Rules _rules;
    rtype::game::GameConfig _config;
    PublishFunction _publish;
    FailureFunction _onFailure;

    std::mutex _mutex;
    std::condition_variable _wake;
    std::vector<Command> _commands;
    bool _stopping = false;
    std::thread _thread;
    std::atomic<std::uint32_t> _tick = 0;

    void push(Command command);
    void run();

public:
    /// @param config The definitions of the game (readGameConfig()), given to its Simulation.
    GameThread(rtype::game::Rules rules, rtype::game::GameConfig config, PublishFunction publish,
               FailureFunction onFailure = {});
    /// Stops the thread, as stop() does.
    ~GameThread();

    GameThread(const GameThread &) = delete;
    GameThread &operator=(const GameThread &) = delete;

    /// Starts the thread. Call it once.
    void start();
    /// Stops the thread after its current tick, and waits for it. Nothing is published after.
    void stop();

    /// Adds the ship of a player, from any thread.
    void addPlayer(std::uint8_t slot);
    /// Removes the ship of a player, from any thread.
    void removePlayer(std::uint8_t slot);
    /// The inputs of an INPUT of a player, from any thread: the input of `tick`, then of `tick - 1`, and so
    /// on. Each one is applied at its tick (InputBuffer, docs/protocol/game.md, section 9.2).
    void receiveInputs(std::uint8_t slot, std::uint32_t tick, std::vector<rtype::game::PlayerInput> inputs);

    /// The last tick simulated, from any thread: for `ACCEPTED`.
    [[nodiscard]] std::uint32_t tick() const { return _tick; }
};

} // namespace rtype::server
