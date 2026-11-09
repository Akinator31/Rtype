#pragma once

#include "rtype/network/Address.hpp"

#include <cstddef>
#include <functional>
#include <span>

namespace rtype::server
{

/// How the datagrams of a game reach its clients and come back from them, for the GameHost of an instance
/// (docs/architecture/server.md#an-instance-at-work). DirectTransport is a UDP socket of its own; the tests
/// give a fake that keeps what is sent and hands datagrams in without a socket.
///
/// Every datagram is the one of the game protocol, unchanged, with the real address of its client: the
/// sessions follow and limit clients by that address, whatever carries the datagram.
class GameTransport
{
public:
    /// Called for each datagram received, on the thread of the loop. The data is only valid during the call.
    using ReceiveHandler =
        std::function<void(const rtype::network::Address &from, std::span<const std::byte> datagram)>;

    virtual ~GameTransport() = default;

    /// Starts receiving: the handler is called for every datagram, until close(). Call it once.
    virtual void start(ReceiveHandler handler) = 0;

    /// Sends a datagram to a client. The data is copied, so it can be reused at once; a datagram that cannot
    /// leave is dropped, like one lost on the way.
    virtual void send(const rtype::network::Address &to, std::span<const std::byte> datagram) = 0;

    /// Stops, after the datagrams already given to send(). The handler is no longer called.
    virtual void close() = 0;

    /// Where the game receives, with the port the system chose if 0 was asked: what the logs show.
    [[nodiscard]] virtual const rtype::network::Address &localAddress() const = 0;
};

} // namespace rtype::server
