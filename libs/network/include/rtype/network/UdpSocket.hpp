#pragma once

#include "rtype/network/Address.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>

namespace rtype::network
{

class EventLoop;

/// A UDP socket: sends and receives datagrams, each one a separate message that may be lost, duplicated or
/// reordered by the network.
///
/// The socket is bound to a local address when it is created, and closed when it is destroyed:
/// @code
/// rtype::network::EventLoop loop;
/// rtype::network::UdpSocket socket(loop, rtype::network::Address::anyIpv4(4242));
///
/// socket.startReceiving([&](const Address &from, std::span<const std::byte> datagram) {
///     socket.send(from, datagram); // echo
/// });
/// loop.run();
/// @endcode
///
/// Every function can be called from any thread: the work is handed to the loop's thread, in the order of the
/// calls. The handler is called on the loop's thread.
class UdpSocket
{
public:
    /// Called for each datagram received, on the loop's thread. The data is only valid during the call.
    using ReceiveHandler = std::function<void(const Address &from, std::span<const std::byte> datagram)>;

    /// Largest datagram a socket can receive. Larger ones are cut by the system.
    static constexpr std::size_t MaxDatagramSize = 65535;

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
    Address _localAddress;

public:
    /// Opens a socket bound to a local address. Port 0 lets the system choose a free port: see
    /// localAddress().
    /// @throws std::system_error if the socket cannot be opened, for instance if the port is already in use.
    UdpSocket(EventLoop &loop, const Address &local);
    /// Closes the socket.
    ~UdpSocket();

    UdpSocket(const UdpSocket &) = delete;
    UdpSocket &operator=(const UdpSocket &) = delete;
    UdpSocket(UdpSocket &&) noexcept;
    UdpSocket &operator=(UdpSocket &&) noexcept;

    /// The address the socket is bound to, with the port the system chose if 0 was given.
    [[nodiscard]] const Address &localAddress() const { return _localAddress; }

    /// Starts receiving: the handler is called for every datagram, until the socket is closed. Call it once.
    void startReceiving(ReceiveHandler handler);

    /// Sends a datagram. The data is copied, so it can be reused at once. The datagram is sent from the
    /// loop's thread; if the system cannot send it (full buffer, unreachable network), it is dropped like a
    /// datagram lost on the way, and logged.
    void send(const Address &to, std::span<const std::byte> datagram);

    /// Closes the socket, after the datagrams already given to send(). The receive handler is no longer
    /// called.
    void close();
};

} // namespace rtype::network
