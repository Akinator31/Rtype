#pragma once

#include "Instance/GameTransport.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <cstdint>

namespace rtype::server
{

/// The transport of a game behind the relay of its master (docs/protocol/instances.md, section 5): the
/// clients send to the one public UDP port of the master, which forwards each datagram in a relay frame, with
/// the address of its client, to a port of the instance on the loopback. Each datagram sent goes back the
/// same way, in a frame to the relay.
///
/// @code
/// auto transport = std::make_unique<rtype::server::RelayedTransport>(loop, configure.relayPort);
/// master.sendReady(transport->localAddress().port()); // where the relay sends its frames
/// @endcode
///
/// Only the frames from the relay are read: anything else on the loopback is dropped, as a frame that does
/// not decode. The game sees the real address of each client, as with a port of its own.
class RelayedTransport final : public GameTransport
{
    rtype::network::UdpSocket _socket;
    rtype::network::Address _relay;

public:
    /// Opens a UDP socket on 127.0.0.1, on a port the system chooses, for the relay on `127.0.0.1:relayPort`.
    /// @throws std::system_error if the socket cannot be opened.
    RelayedTransport(rtype::network::EventLoop &loop, std::uint16_t relayPort);

    void start(ReceiveHandler handler) override;
    /// Wraps the datagram in a frame to the relay. One the frame cannot carry (empty, too large, to port 0)
    /// is dropped.
    void send(const rtype::network::Address &to, std::span<const std::byte> datagram) override;
    void close() override;
    /// The loopback address the frames of the relay come to: the port of READY.
    [[nodiscard]] const rtype::network::Address &localAddress() const override;
    /// The relay, `127.0.0.1:relayPort`.
    [[nodiscard]] const rtype::network::Address &relay() const { return _relay; }
};

} // namespace rtype::server
