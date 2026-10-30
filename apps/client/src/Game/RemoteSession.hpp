#pragma once

#include "Game/GameSession.hpp"
#include "Game/Interpolation.hpp"
#include "Game/Prediction.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/game/RemoteWorld.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// A game online: the session of the player with its game instance, over encrypted UDP
/// (docs/protocol/game.md), with what GAME_JOINED gave (ADR 0015).
///
/// @code
/// games.play(std::make_unique<RemoteSession>(loop, master.withPort(joined.udpPort), joined));
/// @endcode
///
/// - **Inputs**: once per tick (60 per second), it sends INPUT with the inputs of the last 8 ticks, newest
///   first, so that a lost datagram loses no input. Each one is stamped with the tick the game should apply
///   it at, estimated from the latest snapshot and the round-trip time (section 9.2).
/// - **Snapshots**: the parts received and the destructions go into a RemoteWorld, and each world received
///   into an Interpolation; takeSnapshots() gives, each frame, the world drawn 100 ms in the past, between
///   the two snapshots around that time: the entities move at the frame rate, not 30 times per second.
/// - **The ship of the player** is predicted: each input moves it at once, and each snapshot that has it
///   reconciles the prediction with the game (Prediction). It is drawn where it is predicted, the rest where
///   the interpolation draws it.
/// - **The end**: DISCONNECT from the game, a timeout, or GAME_OVER; endReason() tells the player why.
///
/// The loop must be polled, by the game loop of the client.
class RemoteSession final : public GameSession
{
public:
    /// Inputs in each INPUT: the last ticks, so that a lost datagram loses none (docs/protocol/game.md, 9.2).
    static constexpr std::size_t InputHistory = game::InputRedundancy;

private:
    using Clock = network::ClientSession::Clock;

    network::UdpSocket _socket;
    network::Address _game;
    network::ClientSession _session;
    game::RemoteWorld _world;
    Interpolation _interpolation;
    Prediction _prediction;
    std::optional<game::AcceptedMessage> _accepted;
    /// The id of the ship of the player, 0 while it has none.
    std::uint32_t _shipId = 0;
    std::map<std::uint8_t, std::string> _names;

    game::PlayerInput _input;
    /// The inputs sent, newest first.
    std::deque<game::PlayerInput> _history;
    /// The tick stamped on the newest input sent.
    std::uint32_t _inputTick = 0;
    double _pending = 0.0;
    Clock::time_point _snapshotReceived;

    std::vector<game::DestroyedEntity> _destroyed;
    /// The wave WAVE_STARTED told, until the next snapshot taken.
    std::optional<std::uint16_t> _waveStarted;
    std::string _waveMusic;
    bool _over = false;
    std::string _endReason;

    void received(std::uint8_t type, std::span<const std::byte> payload);
    /// Corrects the prediction with a part of a snapshot that has the ship of the player.
    void reconcile(const game::SnapshotPart &part);
    void sendInput();
    [[nodiscard]] std::uint32_t estimateTick() const;

public:
    /// Starts connecting at once.
    /// @param game The address of the instance: the IP of the master, the UDP port of GAME_JOINED.
    /// @throws std::system_error if no UDP socket can be opened.
    RemoteSession(network::EventLoop &loop, const network::Address &game,
                  const lobby::GameJoinedMessage &joined);
    /// Says goodbye to the game (DISCONNECT) if still in it.
    ~RemoteSession() override;

    RemoteSession(const RemoteSession &) = delete;
    RemoteSession &operator=(const RemoteSession &) = delete;

    [[nodiscard]] std::uint8_t localSlot() const override { return _accepted ? _accepted->slot : 0; }
    void setInput(game::PlayerInput input) override { _input = input; }
    void update(float elapsed) override;
    [[nodiscard]] std::vector<game::Snapshot> takeSnapshots() override;
    /// Nothing: online, the game goes on.
    void setPaused(bool /*paused*/) override {}
    [[nodiscard]] bool isOver() const override { return _over; }
    [[nodiscard]] std::optional<std::chrono::milliseconds> ping() const override;

    /// The game accepted the player.
    [[nodiscard]] bool accepted() const { return _accepted.has_value(); }
    /// Why the game ended, for the player.
    [[nodiscard]] const std::string &endReason() const { return _endReason; }
    /// The names of the players, by slot.
    [[nodiscard]] const std::map<std::uint8_t, std::string> &names() const { return _names; }
    /// The world as received, the newest, for the tests.
    [[nodiscard]] const game::RemoteWorld &world() const { return _world; }
    /// The world as drawn, for the tests.
    [[nodiscard]] const Interpolation &interpolation() const { return _interpolation; }
    /// The prediction of the ship of the player, for the tests.
    [[nodiscard]] const Prediction &prediction() const { return _prediction; }
    /// The tick stamped on the newest input sent.
    [[nodiscard]] std::uint32_t inputTick() const { return _inputTick; }
};

} // namespace rtype::client
