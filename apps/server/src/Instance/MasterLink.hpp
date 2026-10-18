#pragma once

#include "Common/InstanceMessages.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace rtype::server
{

/// The connection of an instance to its master, on 127.0.0.1 (docs/protocol/instances.md, section 2): it
/// connects, presents itself with INSTANCE_HELLO, waits for CONFIGURE, then receives the players.
///
/// @code
/// rtype::server::MasterLink link(loop, options.masterPort, {.id = options.id, .secret = secret},
///     {.onConfigure = [&](const ConfigureMessage &game) { link.sendReady(port); },
///      .onAddPlayer = ..., .onRemovePlayer = ..., .onShutdown = ...,
///      .onFailed = [&](const std::string &reason) { /* stop the game */ }});
/// @endcode
///
/// The master and its instances follow the protocol, so anything else is a failure, after which the link is
/// closed: a connection that cannot be opened, no CONFIGURE in time, a frame that does not decode, a message
/// that is not expected (CONFIGURE twice, a player before CONFIGURE), or the master closing the connection.
class MasterLink
{
public:
    struct Handlers {
        /// Once, first: the game to run. Answer sendReady() once the UDP port is open.
        std::function<void(const ConfigureMessage &game)> onConfigure;
        std::function<void(const AddPlayerMessage &player)> onAddPlayer;
        std::function<void(const RemovePlayerMessage &player)> onRemovePlayer;
        std::function<void(const ShutdownMessage &shutdown)> onShutdown;
        /// Once, when the link fails: the instance cannot go on without its master.
        std::function<void(const std::string &reason)> onFailed;
    };

    /// How long the master has to answer CONFIGURE, from the start of the connection (instances protocol,
    /// section 2).
    static constexpr std::chrono::seconds ConfigureTimeout{10};

private:
    InstanceHelloMessage _hello;
    Handlers _handlers;
    rtype::network::Timer _configureTimeout;
    std::optional<rtype::network::FramedConnection> _connection;
    bool _configured = false;
    bool _closed = false;
    /// Dies with the link: the handler of the connection attempt, called later, checks it
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    void connected(rtype::network::TcpConnection connection);
    void received(std::uint8_t type, std::span<const std::byte> payload);
    void fail(const std::string &reason);
    void send(std::uint8_t type, std::span<const std::byte> payload);

public:
    /// Starts connecting to the master, at once, without blocking.
    MasterLink(rtype::network::EventLoop &loop, std::uint16_t masterPort, InstanceHelloMessage hello,
               Handlers handlers, std::chrono::milliseconds configureTimeout = ConfigureTimeout);
    ~MasterLink();

    MasterLink(const MasterLink &) = delete;
    MasterLink &operator=(const MasterLink &) = delete;

    void sendReady(std::uint16_t udpPort);
    void sendPlayerConnected(std::uint32_t playerId);
    void sendPlayerDisconnected(std::uint32_t playerId, PlayerDisconnectedReason reason);
    void sendGameOver(const GameOverReport &report);

    /// Closes the connection after the frames already sent. No handler is called after.
    void close();
};

} // namespace rtype::server
