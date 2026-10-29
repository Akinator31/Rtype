#include "Master/Lobby.hpp"

#include "rtype/lobby/Messages.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <chrono>
#include <deque>
#include <format>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace rtype::server
{

namespace
{

namespace message_type = rtype::lobby::message_type;
using rtype::lobby::ErrorCode;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("lobby");
    return instance;
}

} // namespace

struct Lobby::Client {
    enum class State {
        /// The TLS handshake runs.
        Handshake,
        /// Secured, waiting for HELLO.
        Unwelcomed,
        /// Welcomed: a player in the lobby.
        Welcomed,
    };

    std::uint64_t id;
    rtype::network::Address address;
    rtype::network::FramedConnection connection;
    /// The handshake, HELLO or silence timeout, depending on the state.
    rtype::network::Timer timeout;
    State state = State::Handshake;
    std::uint32_t playerId = 0;
    std::string name;
    std::chrono::system_clock::time_point welcomedAt;
    /// The game the player is in.
    std::optional<std::uint32_t> gameId;
    /// A CREATE_GAME waits for its game.
    bool busy = false;
    /// The requests received meanwhile.
    std::deque<std::pair<std::uint8_t, std::vector<std::byte>>> waiting;
    bool closed = false;

    Client(std::uint64_t id, rtype::network::EventLoop &loop, rtype::network::TlsConnection tls)
        : id(id), address(tls.remoteAddress()), connection(std::move(tls)), timeout(loop)
    {
    }

    [[nodiscard]] std::string describe() const
    {
        return state == State::Welcomed ? std::format("{} ({})", name, playerId) : address.toString();
    }
};

Lobby::Lobby(rtype::network::EventLoop &loop, rtype::network::TcpListener listener,
             rtype::network::ServerIdentity identity, Games &games, LobbyLimits limits)
    : _loop(loop), _listener(std::move(listener)), _identity(std::move(identity)), _games(games),
      _limits(limits)
{
    _games.setLeftHandler(
        [this](std::uint32_t playerId, const std::optional<rtype::lobby::LeftGameMessage> &left) {
            this->left(playerId, left);
        });
    _listener.startAccepting(
        [this](rtype::network::TcpConnection connection) { accept(std::move(connection)); });
}

Lobby::~Lobby()
{
    _games.setLeftHandler({});
    close();
}

void Lobby::accept(rtype::network::TcpConnection connection)
{
    if (_closed) {
        return;
    }
    const rtype::network::Address ip = connection.remoteAddress().withPort(0);
    // Refused before any TLS work, which costs the server more than the client: the connection closes with it
    if (_open >= _limits.maxConnections) {
        logger().warning("Connection from {} refused: {} connections already", connection.remoteAddress(),
                         _open);
        return;
    }
    if (_perAddress[ip] >= _limits.maxConnectionsPerAddress) {
        logger().warning("Connection from {} refused: {} connections from {} already",
                         connection.remoteAddress(), _perAddress[ip], ip.ip());
        return;
    }

    const std::uint64_t id = ++_nextConnection;
    auto client = std::make_unique<Client>(
        id, _loop, rtype::network::TlsConnection::server(std::move(connection), _identity));
    Client &added = *client;
    _clients.emplace(id, std::move(client));
    ++_perAddress[ip];
    ++_open;
    logger().debug("Connection {} from {}", id, added.address);

    added.timeout.start(_limits.handshakeTimeout, [this, id] { disconnect(id, "TLS handshake too long"); });
    added.connection.start(
        [this, id](std::uint8_t type, std::span<const std::byte> payload) { received(id, type, payload); },
        [this, id](std::error_code reason) {
            const auto found = _clients.find(id);
            if (found == _clients.end()) {
                return;
            }
            const Client &client = *found->second;
            if (client.state == Client::State::Welcomed) {
                logger().info("{} left the lobby{}", client.describe(),
                              reason ? std::format(": {}", reason.message()) : "");
            } else {
                logger().debug("Connection {} from {} ended: {}", id, client.address,
                               reason ? reason.message() : "closed");
            }
            forget(id);
        },
        [this, id] { secured(id); });
}

void Lobby::secured(std::uint64_t id)
{
    const auto found = _clients.find(id);
    if (found == _clients.end()) {
        return;
    }
    Client &client = *found->second;
    client.state = Client::State::Unwelcomed;
    client.timeout.start(_limits.helloTimeout, [this, id] { disconnect(id, "no HELLO in time"); });
}

void Lobby::reply(Client &client, std::uint8_t type, std::span<const std::byte> payload)
{
    if (!client.closed) {
        client.connection.send(type, payload);
    }
}

void Lobby::fail(Client &client, std::uint8_t request, ErrorCode code)
{
    reply(client, message_type::Error, rtype::lobby::encode(rtype::lobby::ErrorMessage{request, code}));
    const auto value = static_cast<std::uint16_t>(code);
    if (rtype::lobby::closesConnection(code)) {
        disconnect(client.id, std::format("error {} on request {:#04x}", value, request));
    } else {
        logger().debug("{}: error {} on request {:#04x}", client.describe(), value, request);
    }
}

void Lobby::received(std::uint64_t id, std::uint8_t type, std::span<const std::byte> payload)
{
    const auto found = _clients.find(id);
    if (found == _clients.end() || found->second->closed) {
        return;
    }
    Client &client = *found->second;
    if (client.state == Client::State::Welcomed) {
        // Any frame shows the client is alive
        client.timeout.start(_limits.idleTimeout, [this, id] { disconnect(id, "silent for too long"); });
    }
    if (client.busy) {
        // A CREATE_GAME waits for its game: the requests after it wait for its answer
        if (client.waiting.size() >= _limits.maxWaitingRequests) {
            disconnect(id, "too many requests waiting");
            return;
        }
        client.waiting.emplace_back(type, std::vector<std::byte>(payload.begin(), payload.end()));
        return;
    }
    handle(client, type, payload);
}

void Lobby::drain(std::uint64_t id)
{
    for (;;) {
        const auto found = _clients.find(id);
        if (found == _clients.end() || found->second->closed || found->second->busy ||
            found->second->waiting.empty()) {
            return;
        }
        Client &client = *found->second;
        const auto [type, payload] = std::move(client.waiting.front());
        client.waiting.pop_front();
        handle(client, type, payload);
    }
}

void Lobby::handle(Client &client, std::uint8_t type, std::span<const std::byte> payload)
{
    const bool welcomed = client.state == Client::State::Welcomed;
    const auto malformed = [&] { fail(client, type, ErrorCode::Malformed); };
    const auto error = [&](ErrorCode code) { fail(client, type, code); };
    // For the requests that need HELLO: the state is checked first, the payload after
    const auto notWelcomed = [&] {
        if (!welcomed) {
            error(ErrorCode::HelloRequired);
        }
        return !welcomed;
    };

    switch (type) {
    case message_type::Hello:
        if (welcomed) {
            error(ErrorCode::AlreadyWelcomed);
        } else {
            hello(client, payload);
        }
        return;
    case message_type::Ping:
        if (!rtype::lobby::decodeEmpty(payload)) {
            malformed();
        } else {
            reply(client, message_type::Pong);
        }
        return;
    case message_type::ListGames:
        if (notWelcomed()) {
            return;
        }
        if (!rtype::lobby::decodeEmpty(payload)) {
            malformed();
        } else {
            reply(client, message_type::GameList,
                  rtype::lobby::encode(rtype::lobby::GameListMessage{_games.list()}));
        }
        return;
    case message_type::CreateGame:
        if (notWelcomed()) {
            return;
        }
        if (client.gameId) {
            error(ErrorCode::AlreadyInGame);
        } else {
            createGame(client, payload);
        }
        return;
    case message_type::JoinGame:
        if (notWelcomed()) {
            return;
        }
        if (client.gameId) {
            error(ErrorCode::AlreadyInGame);
        } else if (const auto join = rtype::lobby::decodeJoinGame(payload); !join) {
            malformed();
        } else {
            joined(client, type, _games.join({client.playerId, client.name}, join->gameId));
        }
        return;
    case message_type::LeaveGame:
        if (notWelcomed()) {
            return;
        }
        if (!client.gameId) {
            error(ErrorCode::NotInGame);
        } else if (!rtype::lobby::decodeEmpty(payload)) {
            malformed();
        } else {
            const std::uint32_t gameId = _games.leave(client.playerId).value_or(*client.gameId);
            client.gameId.reset();
            logger().info("{} left game {}", client.describe(), gameId);
            reply(client, message_type::LeftGame,
                  rtype::lobby::encode(
                      rtype::lobby::LeftGameMessage{gameId, rtype::lobby::LeftGameReason::Requested}));
            changed(client);
        }
        return;
    default:
        error(ErrorCode::UnknownMessage);
        return;
    }
}

void Lobby::createGame(Client &client, std::span<const std::byte> payload)
{
    const std::optional<rtype::lobby::CreateGameMessage> game = rtype::lobby::decodeCreateGame(payload);
    if (!game) {
        fail(client, message_type::CreateGame, ErrorCode::Malformed);
        return;
    }
    if (!rtype::lobby::isValidGameName(game->name)) {
        fail(client, message_type::CreateGame, ErrorCode::InvalidName);
        return;
    }
    client.busy = true;
    const std::uint64_t id = client.id;
    const std::uint32_t playerId = client.playerId;
    _games.create({playerId, client.name}, *game,
                  [this, id, playerId, alive = std::weak_ptr<bool>(_alive)](const Games::JoinResult &result) {
                      if (alive.expired()) {
                          return;
                      }
                      const auto found = _clients.find(id);
                      if (found == _clients.end() || found->second->closed) {
                          // Gone while its game started: it leaves it at once
                          if (std::holds_alternative<rtype::lobby::GameJoinedMessage>(result)) {
                              _games.lobbyClosed(playerId);
                          }
                          return;
                      }
                      Client &creator = *found->second;
                      creator.busy = false;
                      joined(creator, message_type::CreateGame, result);
                      drain(id);
                  });
}

void Lobby::joined(Client &client, std::uint8_t request, const Games::JoinResult &result)
{
    if (const auto *game = std::get_if<rtype::lobby::GameJoinedMessage>(&result)) {
        client.gameId = game->gameId;
        reply(client, message_type::GameJoined, rtype::lobby::encode(*game));
        changed(client);
        return;
    }
    fail(client, request, std::get<ErrorCode>(result));
}

void Lobby::left(std::uint32_t playerId, const std::optional<rtype::lobby::LeftGameMessage> &notification)
{
    const auto player = _players.find(playerId);
    if (player == _players.end()) {
        return;
    }
    const auto found = _clients.find(player->second);
    if (found == _clients.end() || found->second->closed) {
        return;
    }
    Client &client = *found->second;
    client.gameId.reset();
    if (notification) {
        reply(client, message_type::LeftGame, rtype::lobby::encode(*notification));
    }
    changed(client);
}

void Lobby::hello(Client &client, std::span<const std::byte> payload)
{
    const std::optional<rtype::lobby::HelloMessage> hello = rtype::lobby::decodeHello(payload);
    if (!hello) {
        fail(client, message_type::Hello, ErrorCode::Malformed);
        return;
    }
    if (hello->version != rtype::lobby::ProtocolVersion) {
        logger().info("{} speaks version {} of the protocol, not {}", client.address, hello->version,
                      rtype::lobby::ProtocolVersion);
        fail(client, message_type::Hello, ErrorCode::UnsupportedVersion);
        return;
    }
    if (_bans != nullptr) {
        if (const std::optional<AddressBan> ban = _bans->find(client.address)) {
            logger().info("{} refused: its address is banned until {:%F %T} UTC", client.address,
                          std::chrono::floor<std::chrono::seconds>(ban->expiresAt));
            fail(client, message_type::Hello, ErrorCode::Banned);
            return;
        }
    }
    if (!rtype::lobby::isValidPlayerName(hello->name)) {
        fail(client, message_type::Hello, ErrorCode::InvalidName);
        return;
    }
    // Never 0, never given twice: 4 billion players before it would wrap
    client.playerId = ++_lastPlayerId;
    client.name = hello->name;
    client.welcomedAt = std::chrono::system_clock::now();
    _players[client.playerId] = client.id;
    client.state = Client::State::Welcomed;
    const std::uint64_t id = client.id;
    client.timeout.start(_limits.idleTimeout, [this, id] { disconnect(id, "silent for too long"); });
    reply(client, message_type::Welcome, rtype::lobby::encode(rtype::lobby::WelcomeMessage{client.playerId}));
    logger().info("{} joined the lobby from {}", client.describe(), client.address);
    if (_observer != nullptr) {
        _observer->playerJoined(client.playerId);
    }
}

void Lobby::disconnect(std::uint64_t id, const std::string &reason)
{
    const auto found = _clients.find(id);
    if (found == _clients.end() || found->second->closed) {
        return;
    }
    Client &client = *found->second;
    if (client.state == Client::State::Welcomed) {
        logger().info("{} disconnected: {}", client.describe(), reason);
    } else {
        logger().debug("Connection {} from {} closed: {}", id, client.address, reason);
    }
    client.connection.close();
    forget(id);
}

void Lobby::forget(std::uint64_t id)
{
    const auto found = _clients.find(id);
    if (found == _clients.end() || found->second->closed) {
        return;
    }
    Client &client = *found->second;
    client.closed = true;
    client.timeout.cancel();
    --_open;
    if (client.state == Client::State::Welcomed) {
        _players.erase(client.playerId);
        // Out of its game, or of the game it was creating
        _games.lobbyClosed(client.playerId);
        if (_observer != nullptr) {
            _observer->playerLeft(client.playerId);
        }
    }
    const rtype::network::Address ip = client.address.withPort(0);
    if (--_perAddress[ip] == 0) {
        _perAddress.erase(ip);
    }
    // Destroyed later: this may run in a handler of its connection or of its timer
    _loop.post([this, id, alive = std::weak_ptr<bool>(_alive)] {
        if (!alive.expired()) {
            _clients.erase(id);
        }
    });
}

Lobby::Client *Lobby::player(std::uint32_t playerId) const
{
    const auto connection = _players.find(playerId);
    if (connection == _players.end()) {
        return nullptr;
    }
    const auto found = _clients.find(connection->second);
    return found == _clients.end() || found->second->closed ? nullptr : found->second.get();
}

void Lobby::changed(const Client &client)
{
    if (_observer != nullptr && !client.closed) {
        _observer->playerChanged(client.playerId);
    }
}

std::vector<AdminPlayer> Lobby::adminPlayers() const
{
    std::vector<AdminPlayer> players;
    players.reserve(_players.size());
    for (const auto &[playerId, connection] : _players) {
        if (const Client *client = player(playerId)) {
            players.push_back({client->playerId, client->name, banKey(client->address), client->gameId,
                               client->welcomedAt});
        }
    }
    return players;
}

bool Lobby::disconnect(std::uint32_t playerId, ErrorCode code)
{
    Client *client = player(playerId);
    if (client == nullptr) {
        return false;
    }
    reply(*client, message_type::Error, rtype::lobby::encode(rtype::lobby::ErrorMessage{0, code}));
    disconnect(client->id,
               code == ErrorCode::Banned ? "its address was banned" : "removed by an administrator");
    return true;
}

bool Lobby::sendMessage(std::uint32_t playerId, const std::string &text)
{
    Client *client = player(playerId);
    if (client == nullptr) {
        return false;
    }
    reply(*client, message_type::ServerMessage,
          rtype::lobby::encode(rtype::lobby::ServerMessageMessage{text}));
    return true;
}

void Lobby::close()
{
    if (_closed) {
        return;
    }
    _closed = true;
    _listener.close();
    if (_open != 0) {
        logger().info("Closing {} connections", _open);
    }
    for (auto &[id, client] : _clients) {
        if (!client->closed) {
            client->closed = true;
            client->timeout.cancel();
            client->connection.close();
        }
    }
    _perAddress.clear();
    _open = 0;
}

} // namespace rtype::server
