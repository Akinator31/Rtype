#pragma once

#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

namespace rtype::server
{

/// The limits the lobby enforces against floods (docs/protocol/index.md, "Limits"). The defaults are those of
/// the protocol; the tests use smaller ones.
struct LobbyLimits {
    /// Connections at the same time, welcomed or not. The next ones are closed at once.
    std::size_t maxConnections = 256;
    /// Connections at the same time from one IP address.
    std::size_t maxConnectionsPerAddress = 8;
    /// From the TCP connection to the end of the TLS handshake.
    std::chrono::milliseconds handshakeTimeout{std::chrono::seconds(10)};
    /// From the end of the handshake to HELLO.
    std::chrono::milliseconds helloTimeout{std::chrono::seconds(10)};
    /// Without any frame from a welcomed client: it sends PING after 10 seconds of silence.
    std::chrono::milliseconds idleTimeout{std::chrono::seconds(30)};
};

/// The lobby of the master: the TLS connections of the clients, and the requests they send
/// (docs/protocol/lobby.md), on the thread of the loop.
///
/// @code
/// rtype::server::Lobby lobby(loop, std::move(listener), identity);
/// // ...
/// lobby.close(); // Ctrl+C: the clients are disconnected
/// @endcode
///
/// - Each connection goes through the states of the protocol: TLS handshake (10 s), then HELLO (10 s), then
///   the lobby, closed after 30 s of silence. Each state accepts its own requests; the others get the ERROR
///   the protocol gives (HELLO_REQUIRED, ALREADY_WELCOMED, NOT_IN_GAME, UNKNOWN_MESSAGE).
/// - Every request gets exactly one reply, in order. A malformed one gets ERROR (MALFORMED), then the
///   connection is closed, as after ERROR (UNSUPPORTED_VERSION).
/// - HELLO gives the player an id, never 0 and never given twice by this master.
/// - For now no game exists: LIST_GAMES gets an empty list, JOIN_GAME GAME_NOT_FOUND, CREATE_GAME
///   GAME_START_FAILED. The games come with `server: games`.
class Lobby
{
    struct Client;

    rtype::network::EventLoop &_loop;
    rtype::network::TcpListener _listener;
    rtype::network::ServerIdentity _identity;
    LobbyLimits _limits;
    std::map<std::uint64_t, std::unique_ptr<Client>> _clients;
    /// Connections by IP address, the port set to 0.
    std::unordered_map<rtype::network::Address, std::size_t> _perAddress;
    /// Connections not closed: those waiting to be destroyed do not count.
    std::size_t _open = 0;
    std::uint64_t _nextConnection = 0;
    std::uint32_t _lastPlayerId = 0;
    bool _closed = false;
    /// Dies with the lobby: what it posts to the loop checks it.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    void accept(rtype::network::TcpConnection connection);
    void secured(std::uint64_t id);
    void received(std::uint64_t id, std::uint8_t type, std::span<const std::byte> payload);
    void hello(Client &client, std::span<const std::byte> payload);
    void reply(Client &client, std::uint8_t type, std::span<const std::byte> payload = {});
    void fail(Client &client, std::uint8_t request, rtype::lobby::ErrorCode code);
    /// Closes a connection, after what was sent to it, and forgets it.
    void disconnect(std::uint64_t id, const std::string &reason);
    /// Forgets a connection, later: it may be the one whose handler runs.
    void forget(std::uint64_t id);

public:
    /// Accepts the clients on this listener, and proves to them it is the server with this identity.
    Lobby(rtype::network::EventLoop &loop, rtype::network::TcpListener listener,
          rtype::network::ServerIdentity identity, LobbyLimits limits = {});
    ~Lobby();

    Lobby(const Lobby &) = delete;
    Lobby &operator=(const Lobby &) = delete;

    [[nodiscard]] const rtype::network::Address &localAddress() const { return _listener.localAddress(); }

    /// Connections open now, welcomed or not.
    [[nodiscard]] std::size_t connectionCount() const { return _open; }

    /// Stops accepting, and closes every connection after what was sent to it: the master stops.
    void close();
};

} // namespace rtype::server
