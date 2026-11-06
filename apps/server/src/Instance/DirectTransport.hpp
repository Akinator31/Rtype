#pragma once

#include "Instance/GameTransport.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/UdpSocket.hpp"

namespace rtype::server
{

/// The transport of a game with a UDP port of its own: the clients send to it, and it answers from it.
///
/// @code
/// auto transport = std::make_unique<rtype::server::DirectTransport>(
///     loop, rtype::network::Address::anyIpv4(udpPort)); // throws if the port is in use
/// rtype::server::GameHost host(loop, std::move(transport), 4, game, handlers);
/// @endcode
class DirectTransport final : public GameTransport
{
    rtype::network::UdpSocket _socket;

public:
    /// Opens the UDP socket of the game.
    /// @throws std::system_error if it cannot be opened: the port is in use.
    DirectTransport(rtype::network::EventLoop &loop, const rtype::network::Address &local);

    void start(ReceiveHandler handler) override;
    void send(const rtype::network::Address &to, std::span<const std::byte> datagram) override;
    void close() override;
    [[nodiscard]] const rtype::network::Address &localAddress() const override;
};

} // namespace rtype::server
