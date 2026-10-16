#pragma once

#include "rtype/network/Address.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <system_error>

namespace rtype::network
{

class EventLoop;
class TcpListener;

/// A TCP connection: a stream of bytes in both directions, complete and in order.
///
/// A stream has no message boundaries: bytes sent in one send() may arrive in several receive calls, and
/// bytes of several send() calls in one. Splitting the stream into messages is the job of the layer above.
///
/// A connection comes from connect() on the client, or from a TcpListener on the server:
/// @code
/// TcpConnection::connect(loop, server, [](std::error_code error, std::optional<TcpConnection> connection) {
///     if (error) {
///         logger.error("Cannot reach the server: {}", error.message());
///         return;
///     }
///     connection->start(
///         [](std::span<const std::byte> bytes) { /* bytes received */ },
///         [](std::error_code reason) { /* closed by the peer, or failed */ });
///     connection->send(hello);
/// });
/// @endcode
///
/// Every function can be called from any thread. The handlers are called on the loop's thread.
class TcpConnection
{
public:
    /// Called with the bytes received, on the loop's thread. The data is only valid during the call.
    using ReceiveHandler = std::function<void(std::span<const std::byte> bytes)>;
    /// Called once when the connection ends without close() being called: the peer closed it (no error) or
    /// it failed (the error).
    using CloseHandler = std::function<void(std::error_code reason)>;
    /// Called once with the connection, or with the reason why it could not be opened.
    using ConnectHandler =
        std::function<void(std::error_code error, std::optional<TcpConnection> connection)>;

    struct Impl;

private:
    friend class TcpListener;

    std::shared_ptr<Impl> _impl;
    Address _localAddress;
    Address _remoteAddress;

    explicit TcpConnection(std::shared_ptr<Impl> impl);

public:
    /// Opens a connection to a server, without blocking: the handler is called on the loop's thread when the
    /// connection is open, or fails.
    /// @param timeout Delay after which the attempt fails with `std::errc::timed_out`.
    static void connect(EventLoop &loop, const Address &remote, ConnectHandler handler,
                        std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /// Closes the connection, as close() does.
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;
    TcpConnection(TcpConnection &&) noexcept;
    TcpConnection &operator=(TcpConnection &&) noexcept;

    /// Address of this end of the connection.
    [[nodiscard]] const Address &localAddress() const { return _localAddress; }
    /// Address of the peer.
    [[nodiscard]] const Address &remoteAddress() const { return _remoteAddress; }

    /// Starts receiving: the receive handler is called with the bytes as they arrive, and the close handler
    /// when the connection ends. Call it once.
    void start(ReceiveHandler onReceive, CloseHandler onClose);

    /// Sends bytes, after those of the previous calls. The data is copied, so it can be reused at once.
    void send(std::span<const std::byte> bytes);

    /// Closes the connection once the bytes already given to send() are sent. The handlers are no longer
    /// called.
    void close();
};

} // namespace rtype::network
