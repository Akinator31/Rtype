#include "Instance/GameHost.hpp"

#include "rtype/game/Messages.hpp"
#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <chrono>

namespace rtype::server
{

namespace
{

using rtype::network::Delivery;
using rtype::network::DisconnectReason;
using rtype::network::SessionEnd;
using Clock = rtype::network::SessionHost::Clock;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("instance");
    return instance;
}

} // namespace

GameHost::GameHost(rtype::network::EventLoop &loop, const rtype::network::Address &local,
                   std::uint8_t maxPlayers, GameThread &game, Handlers handlers)
    : _socket(loop, local), _game(game), _maxPlayers(std::min(maxPlayers, MaxInstancePlayers)),
      _handlers(std::move(handlers)),
      _sessions(
          [this](const rtype::network::Address &to, std::span<const std::byte> datagram) {
              _socket.send(to, datagram);
          },
          {
              .onConnected = [this](std::uint32_t session,
                                    const rtype::network::Address &from) { connected(session, from); },
              .onMessage = [this](std::uint32_t session, std::uint8_t type,
                                  std::span<const std::byte> payload) { received(session, type, payload); },
              .onClosed = [this](std::uint32_t session, SessionEnd end) { closed(session, end); },
          }),
      _updates(loop)
{
    _socket.startReceiving([this](const rtype::network::Address &from, std::span<const std::byte> datagram) {
        _sessions.receive(from, datagram, Clock::now());
    });
    // Sends, resends, acknowledgements, heartbeats and timeouts, at the pace of the ticks, but on its own:
    // the network never waits for the game
    _updates.startRepeating(GameThread::TickInterval, [this] { _sessions.update(Clock::now()); });
}

std::optional<std::uint8_t> GameHost::freeSlot() const
{
    for (std::uint8_t slot = 0; slot < _maxPlayers; ++slot) {
        if (std::ranges::none_of(_players, [slot](const auto &entry) { return entry.second.slot == slot; })) {
            return slot;
        }
    }
    return std::nullopt;
}

bool GameHost::addPlayer(const AddPlayerMessage &player)
{
    const bool known = std::ranges::any_of(
        _players, [&](const auto &entry) { return entry.second.playerId == player.playerId; });
    const std::optional<std::uint8_t> slot = freeSlot();
    if (known || _players.contains(player.sessionId) || !slot) {
        return false;
    }
    _sessions.add(player.sessionId, rtype::network::DatagramKey(player.clientKey),
                  rtype::network::DatagramKey(player.serverKey), Clock::now());
    _players.emplace(player.sessionId, Player{player.playerId, *slot, player.name, false});
    logger().debug("Player {} ({}) expected on session {}, slot {}", player.name, player.playerId,
                   player.sessionId, *slot);
    return true;
}

void GameHost::connected(std::uint32_t session, const rtype::network::Address &from)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    Player &player = found->second;
    player.connected = true;
    _game.addPlayer(player.slot);
    logger().info("{} ({}) connected from {}, slot {}", player.name, player.playerId, from, player.slot);

    _sessions.send(
        session, rtype::game::message_type::Accepted,
        rtype::game::encode(rtype::game::AcceptedMessage{player.slot, rtype::game::TickRate, _game.tick()}),
        Delivery::Reliable);
    // The players already there, for the newcomer
    for (const auto &[otherSession, other] : _players) {
        if (other.connected && otherSession != session) {
            _sessions.send(
                session, rtype::game::message_type::PlayerJoined,
                rtype::game::encode(rtype::game::PlayerJoinedMessage{other.slot, other.playerId, other.name}),
                Delivery::Reliable);
        }
    }
    // The newcomer, for everyone, itself included
    sendToConnected(
        rtype::game::message_type::PlayerJoined,
        rtype::game::encode(rtype::game::PlayerJoinedMessage{player.slot, player.playerId, player.name}),
        Delivery::Reliable);
    if (_handlers.onConnected) {
        _handlers.onConnected(player.playerId);
    }
}

void GameHost::received(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    if (type != rtype::game::message_type::Input) {
        logger().debug("Message {:#04x} of session {} ignored: not one a client sends", type, session);
        return;
    }
    std::optional<rtype::game::InputMessage> input = rtype::game::decodeInput(payload);
    if (!input) {
        logger().debug("Malformed INPUT of session {} dropped", session);
        return;
    }
    // Each one applied at its tick: the older ones are those a lost datagram did not bring
    _game.receiveInputs(found->second.slot, input->tick, std::move(input->inputs));
}

void GameHost::leave(std::uint32_t session, rtype::game::LeaveReason reason)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    const Player player = found->second;
    _players.erase(found);
    if (player.connected) {
        _game.removePlayer(player.slot);
        sendToConnected(rtype::game::message_type::PlayerLeft,
                        rtype::game::encode(rtype::game::PlayerLeftMessage{player.slot, reason}),
                        Delivery::Reliable);
    }
}

void GameHost::closed(std::uint32_t session, SessionEnd end)
{
    const auto found = _players.find(session);
    if (found == _players.end()) {
        return;
    }
    const Player player = found->second;
    const bool quit = end.cause == SessionEnd::Cause::Disconnected;
    if (player.connected) {
        logger().info("{} ({}) {}", player.name, player.playerId, quit ? "left" : "lost the connection");
    } else {
        logger().info("{} ({}) never connected", player.name, player.playerId);
    }
    leave(session, quit ? rtype::game::LeaveReason::Quit : rtype::game::LeaveReason::TimedOut);
    if (_handlers.onDisconnected) {
        _handlers.onDisconnected(player.playerId,
                                 quit ? PlayerDisconnectedReason::Quit : PlayerDisconnectedReason::TimedOut);
    }
}

bool GameHost::removePlayer(std::uint32_t playerId, RemovePlayerReason reason)
{
    const auto found = std::ranges::find_if(
        _players, [playerId](const auto &entry) { return entry.second.playerId == playerId; });
    if (found == _players.end()) {
        return false;
    }
    const std::uint32_t session = found->first;
    logger().info("{} ({}) removed by the master", found->second.name, playerId);
    _sessions.close(session, DisconnectReason::Quit);
    leave(session, reason == RemovePlayerReason::LobbyClosed ? rtype::game::LeaveReason::LobbyClosed
                                                             : rtype::game::LeaveReason::Quit);
    return true;
}

void GameHost::sendToConnected(std::uint8_t type, std::span<const std::byte> payload, Delivery delivery,
                               std::optional<std::uint32_t> except)
{
    for (const auto &[session, player] : _players) {
        if (player.connected && session != except) {
            _sessions.send(session, type, payload, delivery);
        }
    }
}

void GameHost::publish(const TickResult &result)
{
    if (_over) {
        return;
    }
    const rtype::game::Snapshot &snapshot = result.snapshot;
    for (const rtype::game::EntitiesDestroyedMessage &message : rtype::game::destroyedMessages(snapshot)) {
        sendToConnected(rtype::game::message_type::EntitiesDestroyed, rtype::game::encode(message),
                        Delivery::Reliable);
    }
    // The last snapshot goes with the game over, whatever its tick: the final scores
    if (snapshot.tick % SnapshotInterval == 0 || snapshot.gameOver) {
        for (const auto &[session, player] : _players) {
            if (!player.connected) {
                continue;
            }
            for (const rtype::game::SnapshotPart &part :
                 rtype::game::splitSnapshot(snapshot, result.lastInputs[player.slot])) {
                _sessions.send(session, rtype::game::message_type::Snapshot, rtype::game::encode(part),
                               Delivery::Unreliable);
            }
        }
    }
    if (snapshot.gameOver) {
        gameOver(result);
    }
    // Sent now: waiting for the next update of the sessions would add up to a tick of delay, never the same
    _sessions.update(Clock::now());
}

void GameHost::gameOver(const TickResult &result)
{
    _over = true;
    const rtype::game::GameOutcome outcome = *result.snapshot.gameOver;
    logger().info("Game over: {}", outcome == rtype::game::GameOutcome::Victory ? "victory" : "defeat");
    sendToConnected(rtype::game::message_type::GameOver,
                    rtype::game::encode(rtype::game::GameOverMessage{outcome}), Delivery::Reliable);

    GameOverReport report;
    for (const auto &[session, player] : _players) {
        const auto state =
            std::ranges::find(result.snapshot.players, player.slot, &rtype::game::PlayerState::slot);
        if (player.connected && state != result.snapshot.players.end()) {
            report.scores.push_back({player.playerId, state->score});
        }
    }
    if (_handlers.onGameOver) {
        _handlers.onGameOver(report);
    }
}

bool GameHost::acknowledged() const
{
    return std::ranges::all_of(_players, [this](const auto &entry) {
        return !entry.second.connected || _sessions.pendingReliable(entry.first) == 0;
    });
}

void GameHost::closeAll(DisconnectReason reason)
{
    _updates.cancel();
    _sessions.closeAll(reason);
    _players.clear();
    _socket.close();
}

} // namespace rtype::server
