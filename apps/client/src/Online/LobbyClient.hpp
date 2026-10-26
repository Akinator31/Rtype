#pragma once

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace rtype::client
{

/// What an error of the master means, for the player: "The game is full".
[[nodiscard]] std::string describe(lobby::ErrorCode code);

/// The connection of the client to the lobby of a master (docs/protocol/lobby.md): TCP, TLS with the keys
/// trusted for the server pinned, then HELLO, then the requests of the player.
///
/// @code
/// LobbyClient lobby(loop, server, {pinned}, "Pavel",
///                   {.onWelcome = [&](std::uint32_t player) { lobby.listGames(...); },
///                    .onLeftGame = [&](const lobby::LeftGameMessage &left) { /* back to the list */ },
///                    .onClosed = [&](const std::string &reason) { /* toast it */ }});
/// lobby.joinGame(id, [&](const LobbyClient::JoinResult &result) { /* GAME_JOINED or an error */ });
/// @endcode
///
/// - The master answers the requests in order: each reply goes to the callback of its request.
/// - A server that does not have a trusted key is refused during the handshake, before HELLO leaves:
/// onUntrusted
///   tells the key it presented, so that the player can trust it (ADR 0021), or onClosed without it.
/// - After 10 seconds without sending anything, it sends PING, so that the master keeps the connection.
/// - Everything runs on the thread that polls the loop: in the client, the game loop.
class LobbyClient
{
public:
    using JoinResult = std::variant<lobby::GameJoinedMessage, lobby::ErrorCode>;
    using ListCallback = std::function<void(const std::vector<lobby::GameInfo> &games)>;
    using JoinCallback = std::function<void(const JoinResult &result)>;

    struct Handlers {
        /// The master welcomed the player: requests can be sent.
        std::function<void(std::uint32_t playerId)> onWelcome;
        /// The player is no longer in its game without having asked: game over, crash, timeout.
        std::function<void(const lobby::LeftGameMessage &left)> onLeftGame;
        /// The connection ended, or could not be opened, with the reason for the player. Called once; nothing
        /// is called after.
        std::function<void(const std::string &reason)> onClosed;
        /// The server presented a key that is not trusted, which is given: called instead of onClosed if set.
        std::function<void(const network::KeyFingerprint &presented)> onUntrusted;
    };

    /// Sent PING after this long without sending anything (docs/protocol/lobby.md, PING).
    static constexpr std::chrono::seconds PingInterval{10};

private:
    struct Pending {
        std::uint8_t request = 0;
        std::function<void(std::uint8_t type, std::span<const std::byte> payload)> reply;
    };

    std::string _name;
    Handlers _handlers;
    std::optional<network::FramedConnection> _connection;
    network::Timer _ping;
    std::deque<Pending> _pending;
    std::optional<std::uint32_t> _playerId;
    bool _closed = false;
    /// The key the server presented, if it was refused.
    std::optional<network::KeyFingerprint> _untrusted;
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    void connected(network::TcpConnection connection, std::vector<network::KeyFingerprint> trusted);
    void connectionClosed(std::error_code reason);
    void received(std::uint8_t type, std::span<const std::byte> payload);
    void request(std::uint8_t type, std::span<const std::byte> payload,
                 std::function<void(std::uint8_t type, std::span<const std::byte> payload)> reply);
    void fail(const std::string &reason);

public:
    /// Starts connecting at once, without blocking.
    /// @param trusted The keys the server may present (KnownServers::trusted()). None: the server is refused,
    /// and onUntrusted tells its key.
    /// @throws std::invalid_argument if the name cannot be the name of a player (lobby::isValidPlayerName()).
    LobbyClient(network::EventLoop &loop, const network::Address &server,
                std::vector<network::KeyFingerprint> trusted, std::string name, Handlers handlers);
    ~LobbyClient();

    LobbyClient(const LobbyClient &) = delete;
    LobbyClient &operator=(const LobbyClient &) = delete;

    /// The id the master gave the player, once welcomed.
    [[nodiscard]] std::optional<std::uint32_t> playerId() const { return _playerId; }
    [[nodiscard]] bool welcomed() const { return _playerId.has_value() && !_closed; }
    [[nodiscard]] const std::string &name() const { return _name; }

    void listGames(ListCallback done);
    void createGame(const std::string &name, std::uint8_t maxPlayers, JoinCallback done);
    void joinGame(std::uint32_t gameId, JoinCallback done);
    /// LEAVE_GAME. `done` is called once the master answered, whatever it answered.
    void leaveGame(std::function<void()> done = {});

    /// Closes the connection, after what was sent. No handler is called after.
    void close();
};

} // namespace rtype::client
