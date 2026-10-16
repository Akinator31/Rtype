#pragma once

#include "rtype/network/Address.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <functional>
#include <memory>

namespace rtype::network
{

class EventLoop;

/// Listens on a TCP port and accepts the connections of the clients.
///
/// @code
/// TcpListener listener(loop, Address::anyIpv4(4242));
/// listener.startAccepting([&](TcpConnection connection) {
///     logger.info("Client connected from {}", connection.remoteAddress());
///     clients.push_back(std::move(connection));
/// });
/// @endcode
///
/// Every function can be called from any thread. The handler is called on the loop's thread.
class TcpListener
{
public:
    /// Called with each new connection, on the loop's thread. Keep the connection, or it is closed at once.
    using AcceptHandler = std::function<void(TcpConnection connection)>;

    struct Impl;

private:
    std::shared_ptr<Impl> _impl;
    Address _localAddress;

public:
    /// Starts listening on a local address. Port 0 lets the system choose a free port: see localAddress().
    /// @throws std::system_error if the port cannot be used, for instance if it is already in use.
    TcpListener(EventLoop &loop, const Address &local);
    /// Stops listening. Connections already accepted stay open.
    ~TcpListener();

    TcpListener(const TcpListener &) = delete;
    TcpListener &operator=(const TcpListener &) = delete;
    TcpListener(TcpListener &&) noexcept;
    TcpListener &operator=(TcpListener &&) noexcept;

    /// The address the listener is bound to, with the port the system chose if 0 was given.
    [[nodiscard]] const Address &localAddress() const { return _localAddress; }

    /// Starts accepting: the handler is called for every new connection, until the listener is closed. Call
    /// it once.
    void startAccepting(AcceptHandler handler);

    /// Stops listening. Connections already accepted stay open.
    void close();
};

} // namespace rtype::network
