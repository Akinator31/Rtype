#pragma once

#include "Common/InstanceMessages.hpp"
#include "Instance/GameThread.hpp"
#include "rtype/game/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace rtype::server
{

/// The network side of a game, on the network thread of an instance: its UDP socket, the sessions of its
/// players, their slots, and the messages of the game protocol (docs/protocol/game.md). It hands the
/// players and their inputs to the GameThread, and sends the result of each tick to the clients.
///
/// @code
/// rtype::server::GameHost host(loop, rtype::network::Address::anyIpv4(udpPort), 4, game,
///     {.onConnected = [&](std::uint32_t player) { link.sendPlayerConnected(player); },
///      .onDisconnected = [&](std::uint32_t player, auto reason) { ... }});
/// host.addPlayer(addPlayer);       // ADD_PLAYER from the master
/// host.publish(result);            // each tick, from the GameThread through loop.post()
/// @endcode
///
/// - A player gets the lowest free slot when added, and its ship when its client connects.
/// - On `CONNECT`, the client receives `ACCEPTED` (its slot, the tick rate, the tick), then `PLAYER_JOINED`
///   for each player connected, itself included; the others receive its `PLAYER_JOINED`.
/// - The newest input of each `INPUT` goes to the game.
/// - Every 2 ticks, each client receives the snapshot in parts; every tick, the destroyed entities.
/// - A session that closes takes the ship away, and the others receive `PLAYER_LEFT`.
class GameHost
{
public:
    struct Handlers {
        /// The client of a player connected (`PLAYER_CONNECTED` for the master).
        std::function<void(std::uint32_t playerId)> onConnected;
        /// The session of a player closed on the side of the game, or never connected
        /// (`PLAYER_DISCONNECTED`). Not called for removePlayer() and closeAll().
        std::function<void(std::uint32_t playerId, PlayerDisconnectedReason reason)> onDisconnected;
    };

    /// Snapshots are sent once every this many ticks: 30 per second (docs/protocol/game.md, section 9.3).
    static constexpr std::uint32_t SnapshotInterval = 2;

private:
    struct Player {
        std::uint32_t playerId = 0;
        std::uint8_t slot = 0;
        std::string name;
        bool connected = false;
    };

    rtype::network::UdpSocket _socket;
    GameThread &_game;
    std::uint8_t _maxPlayers;
    Handlers _handlers;
    rtype::network::SessionHost _sessions;
    rtype::network::Timer _updates;
    /// By session id.
    std::map<std::uint32_t, Player> _players;

    [[nodiscard]] std::optional<std::uint8_t> freeSlot() const;
    void connected(std::uint32_t session, const rtype::network::Address &from);
    void received(std::uint32_t session, std::uint8_t type, std::span<const std::byte> payload);
    void closed(std::uint32_t session, rtype::network::SessionEnd end);
    /// The player leaves: its ship goes, the others are told.
    void leave(std::uint32_t session, rtype::game::LeaveReason reason);
    void sendToConnected(std::uint8_t type, std::span<const std::byte> payload,
                         rtype::network::Delivery delivery,
                         std::optional<std::uint32_t> except = std::nullopt);

public:
    /// Opens the UDP socket of the game.
    /// @throws std::system_error if it cannot be opened: the port is in use.
    GameHost(rtype::network::EventLoop &loop, const rtype::network::Address &local, std::uint8_t maxPlayers,
             GameThread &game, Handlers handlers);

    GameHost(const GameHost &) = delete;
    GameHost &operator=(const GameHost &) = delete;

    [[nodiscard]] const rtype::network::Address &localAddress() const { return _socket.localAddress(); }

    /// Expects the client of a player on its session.
    /// @return false if the game is full, or the player or its session already exists.
    bool addPlayer(const AddPlayerMessage &player);

    /// Closes the session of a player (`DISCONNECT (QUIT)`), and tells the others.
    /// @return false if the player is unknown: its session had already closed.
    bool removePlayer(std::uint32_t playerId, RemovePlayerReason reason);

    /// Sends the result of a tick to the clients. On the network thread.
    void publish(const TickResult &result);

    /// Closes every session with `DISCONNECT` and this reason, then the socket, after the datagrams queued:
    /// the game stops.
    void closeAll(rtype::network::DisconnectReason reason);

    /// Number of players added, connected or not.
    [[nodiscard]] std::size_t playerCount() const { return _players.size(); }
};

} // namespace rtype::server
