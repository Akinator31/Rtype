#include "Instance/MasterLink.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <format>
#include <system_error>
#include <utility>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("instance");
    return instance;
}

} // namespace

MasterLink::MasterLink(rtype::network::EventLoop &loop, std::uint16_t masterPort, InstanceHelloMessage hello,
                       Handlers handlers, std::chrono::milliseconds configureTimeout)
    : _hello(hello), _handlers(std::move(handlers)), _configureTimeout(loop)
{
    const rtype::network::Address master = rtype::network::Address::loopbackIpv4(masterPort);
    _configureTimeout.start(configureTimeout, [this] { fail("no CONFIGURE from the master in time"); });
    rtype::network::TcpConnection::connect(
        loop, master,
        [this, alive = std::weak_ptr<bool>(_alive),
         master](std::error_code error, std::optional<rtype::network::TcpConnection> connection) {
            if (alive.expired()) {
                return;
            }
            if (error || !connection) {
                fail(std::format("cannot connect to the master on {}: {}", master, error.message()));
                return;
            }
            connected(std::move(*connection));
        },
        configureTimeout);
}

MasterLink::~MasterLink()
{
    close();
}

void MasterLink::connected(rtype::network::TcpConnection connection)
{
    if (_closed) {
        return;
    }
    _connection.emplace(std::move(connection));
    _connection->start(
        [this](std::uint8_t type, std::span<const std::byte> payload) { received(type, payload); },
        [this](std::error_code reason) {
            fail(reason ? std::format("connection to the master lost: {}", reason.message())
                        : std::string("the master closed the connection"));
        });
    logger().debug("Connected to the master, presenting instance {}", _hello.id);
    send(instance_message::InstanceHello, encode(_hello));
}

void MasterLink::received(std::uint8_t type, std::span<const std::byte> payload)
{
    if (!_configured) {
        if (type != instance_message::Configure) {
            fail(std::format("message {:#04x} from the master before CONFIGURE", type));
            return;
        }
        const std::optional<ConfigureMessage> game = decodeConfigure(payload);
        if (!game) {
            fail("malformed CONFIGURE from the master");
            return;
        }
        _configured = true;
        _configureTimeout.cancel();
        if (_handlers.onConfigure) {
            _handlers.onConfigure(*game);
        }
        return;
    }

    switch (type) {
    case instance_message::AddPlayer:
        if (const auto player = decodeAddPlayer(payload)) {
            if (_handlers.onAddPlayer) {
                _handlers.onAddPlayer(*player);
            }
            return;
        }
        break;
    case instance_message::RemovePlayer:
        if (const auto player = decodeRemovePlayer(payload)) {
            if (_handlers.onRemovePlayer) {
                _handlers.onRemovePlayer(*player);
            }
            return;
        }
        break;
    case instance_message::Shutdown:
        if (const auto shutdown = decodeShutdown(payload)) {
            if (_handlers.onShutdown) {
                _handlers.onShutdown(*shutdown);
            }
            return;
        }
        break;
    default:
        fail(std::format("unexpected message {:#04x} from the master", type));
        return;
    }
    fail(std::format("malformed message {:#04x} from the master", type));
}

void MasterLink::fail(const std::string &reason)
{
    if (_closed) {
        return;
    }
    close();
    if (_handlers.onFailed) {
        _handlers.onFailed(reason);
    }
}

void MasterLink::send(std::uint8_t type, std::span<const std::byte> payload)
{
    if (_connection && !_closed) {
        _connection->send(type, payload);
    }
}

void MasterLink::sendReady(std::uint16_t udpPort)
{
    send(instance_message::Ready, encode(ReadyMessage{udpPort}));
}

void MasterLink::sendPlayerConnected(std::uint32_t playerId)
{
    send(instance_message::PlayerConnected, encode(PlayerConnectedMessage{playerId}));
}

void MasterLink::sendPlayerDisconnected(std::uint32_t playerId, PlayerDisconnectedReason reason)
{
    send(instance_message::PlayerDisconnected, encode(PlayerDisconnectedMessage{playerId, reason}));
}

void MasterLink::sendGameOver(const GameOverReport &report)
{
    send(instance_message::GameOver, encode(report));
}

void MasterLink::close()
{
    if (_closed) {
        return;
    }
    _closed = true;
    _configureTimeout.cancel();
    if (_connection) {
        _connection->close();
    }
}

} // namespace rtype::server
