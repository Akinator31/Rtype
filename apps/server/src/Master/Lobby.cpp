#include "Master/Lobby.hpp"

#include "rtype/lobby/Messages.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/TlsConnection.hpp"

#include <format>
#include <utility>

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
             rtype::network::ServerIdentity identity, LobbyLimits limits)
    : _loop(loop), _listener(std::move(listener)), _identity(std::move(identity)), _limits(limits)
{
    _listener.startAccepting(
        [this](rtype::network::TcpConnection connection) { accept(std::move(connection)); });
}

Lobby::~Lobby()
{
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
    const bool welcomed = client.state == Client::State::Welcomed;
    if (welcomed) {
        // Any frame shows the client is alive
        client.timeout.start(_limits.idleTimeout, [this, id] { disconnect(id, "silent for too long"); });
    }
    const auto malformed = [&] { fail(client, type, ErrorCode::Malformed); };
    const auto error = [&](ErrorCode code) { fail(client, type, code); };

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
    case message_type::CreateGame:
    case message_type::JoinGame:
    case message_type::LeaveGame:
        break;
    default:
        error(ErrorCode::UnknownMessage);
        return;
    }

    if (!welcomed) {
        error(ErrorCode::HelloRequired);
        return;
    }
    switch (type) {
    case message_type::ListGames:
        if (!rtype::lobby::decodeEmpty(payload)) {
            malformed();
        } else {
            reply(client, message_type::GameList, rtype::lobby::encode(rtype::lobby::GameListMessage{}));
        }
        return;
    case message_type::CreateGame:
        if (const auto game = rtype::lobby::decodeCreateGame(payload); !game) {
            malformed();
        } else if (!rtype::lobby::isValidGameName(game->name)) {
            error(ErrorCode::InvalidName);
        } else {
            // The instances come with `server: games`
            error(ErrorCode::GameStartFailed);
        }
        return;
    case message_type::JoinGame:
        if (!rtype::lobby::decodeJoinGame(payload)) {
            malformed();
        } else {
            error(ErrorCode::GameNotFound);
        }
        return;
    case message_type::LeaveGame:
        if (!rtype::lobby::decodeEmpty(payload)) {
            malformed();
        } else {
            error(ErrorCode::NotInGame);
        }
        return;
    default:
        return;
    }
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
    if (!rtype::lobby::isValidPlayerName(hello->name)) {
        fail(client, message_type::Hello, ErrorCode::InvalidName);
        return;
    }
    // Never 0, never given twice: 4 billion players before it would wrap
    client.playerId = ++_lastPlayerId;
    client.name = hello->name;
    client.state = Client::State::Welcomed;
    const std::uint64_t id = client.id;
    client.timeout.start(_limits.idleTimeout, [this, id] { disconnect(id, "silent for too long"); });
    reply(client, message_type::Welcome, rtype::lobby::encode(rtype::lobby::WelcomeMessage{client.playerId}));
    logger().info("{} joined the lobby from {}", client.describe(), client.address);
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
