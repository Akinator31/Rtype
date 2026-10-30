#pragma once

#include "Accounts/AccountService.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminModel.hpp"
#include "Master/Games.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
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
    /// Requests sent while one waits for its answer (CREATE_GAME for its game, a request of the accounts for
    /// the database, the hasher or the mailer), kept to be answered in order. Beyond, the client floods: it
    /// is disconnected.
    std::size_t maxWaitingRequests = 32;
};

/// The lobby of the master: the TLS connections of the clients, and the requests they send
/// (docs/protocol/lobby.md), on the thread of the loop.
///
/// @code
/// rtype::server::Lobby lobby(loop, std::move(listener), identity, games);
/// // ...
/// lobby.close(); // Ctrl+C: the clients are disconnected
/// @endcode
///
/// - Each connection goes through the states of the protocol: TLS handshake (10 s), then HELLO (10 s), then
///   the lobby and the games, closed after 30 s of silence. Each state accepts its own requests; the others
///   get the ERROR the protocol gives (HELLO_REQUIRED, ALREADY_WELCOMED, ALREADY_IN_GAME, NOT_IN_GAME,
///   UNKNOWN_MESSAGE).
/// - Every request gets exactly one reply, in order: the requests that follow a CREATE_GAME wait for its
///   game, those that follow a HELLO or a request of the accounts wait for its answer. A malformed one gets
///   ERROR (MALFORMED), then the connection is closed, as after ERROR (UNSUPPORTED_VERSION).
/// - With accounts (setAccounts()), the requests of the accounts go to the AccountService, a HELLO with a
/// token
///   resumes its session, and a guest cannot take the username of an account and carries GuestMark before its
///   name. Without, they get ACCOUNTS_DISABLED, and a token SESSION_EXPIRED.
/// - HELLO gives the player an id, never 0 and never given twice by this master.
/// - The games are those of Games: LIST_GAMES, CREATE_GAME, JOIN_GAME and LEAVE_GAME go to it, and a player
///   who leaves its game without asking (game over, crash, timeout, an administrator) is told with LEFT_GAME.
/// - An address with a ban (setBans()) is refused at HELLO: ERROR (BANNED), then the connection closes.
/// - The administration sees the players and acts on them through AdminLobby, and is told who comes and goes
///   through its AdminObserver (docs/architecture/administration.md).
class Lobby final : public AdminLobby
{
    struct Client;

    rtype::network::EventLoop &_loop;
    rtype::network::TcpListener _listener;
    rtype::network::ServerIdentity _identity;
    Games &_games;
    LobbyLimits _limits;
    const AddressBans *_bans = nullptr;
    AccountService *_accounts = nullptr;
    AdminObserver *_observer = nullptr;
    std::map<std::uint64_t, std::unique_ptr<Client>> _clients;
    /// The connection of each welcomed player.
    std::map<std::uint32_t, std::uint64_t> _players;
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
    /// Answers one request.
    void handle(Client &client, std::uint8_t type, std::span<const std::byte> payload);
    /// Answers the requests that waited for an answer.
    void drain(std::uint64_t id);
    /// Calls `then` with the connection once an asynchronous answer comes, if it is still open, and lets its
    /// next requests through.
    [[nodiscard]] std::function<void(std::function<void(Client &)>)> waitFor(Client &client);
    void hello(Client &client, std::span<const std::byte> payload);
    /// Welcomes a connection, as a guest or signed in.
    void welcome(Client &client, std::string name, std::optional<SignedInAccount> account);
    void accountRequest(Client &client, std::uint8_t type, std::span<const std::byte> payload);
    void createGame(Client &client, std::span<const std::byte> payload);
    void joined(Client &client, std::uint8_t request, const Games::JoinResult &result);
    /// A player left its game without asking.
    void left(std::uint32_t playerId, const std::optional<rtype::lobby::LeftGameMessage> &notification);
    void reply(Client &client, std::uint8_t type, std::span<const std::byte> payload = {});
    void fail(Client &client, std::uint8_t request, rtype::lobby::ErrorCode code);
    /// Closes a connection, after what was sent to it, and forgets it.
    void disconnect(std::uint64_t id, const std::string &reason);
    /// Forgets a connection, later: it may be the one whose handler runs.
    void forget(std::uint64_t id);
    /// The connection of a welcomed player, open.
    [[nodiscard]] Client *player(std::uint32_t playerId) const;
    /// The administration is told a player entered or left a game.
    void changed(const Client &client);

public:
    /// Accepts the clients on this listener, and proves to them it is the server with this identity. The
    /// games must outlive the lobby.
    Lobby(rtype::network::EventLoop &loop, rtype::network::TcpListener listener,
          rtype::network::ServerIdentity identity, Games &games, LobbyLimits limits = {});
    ~Lobby() override;

    Lobby(const Lobby &) = delete;
    Lobby &operator=(const Lobby &) = delete;
    Lobby(Lobby &&) = delete;
    Lobby &operator=(Lobby &&) = delete;

    [[nodiscard]] const rtype::network::Address &localAddress() const { return _listener.localAddress(); }

    /// Connections open now, welcomed or not.
    [[nodiscard]] std::size_t connectionCount() const { return _open; }

    /// Stops accepting, and closes every connection after what was sent to it: the master stops.
    void close();

    /// The bans checked at HELLO; nullptr for none. They must outlive the lobby, or be taken away first.
    void setBans(const AddressBans *bans) { _bans = bans; }
    /// The accounts, before the first client; nullptr for none. They must outlive the lobby.
    void setAccounts(AccountService *accounts) { _accounts = accounts; }
    /// Tells the administration who comes and goes; nullptr for nobody. It must outlive the lobby, or be
    /// taken away first.
    void setObserver(AdminObserver *observer) { _observer = observer; }

    [[nodiscard]] std::vector<AdminPlayer> adminPlayers() const override;
    bool disconnect(std::uint32_t playerId, rtype::lobby::ErrorCode code) override;
    bool sendMessage(std::uint32_t playerId, const std::string &text) override;
};

} // namespace rtype::server
