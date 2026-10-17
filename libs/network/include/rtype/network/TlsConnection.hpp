#pragma once

#include "rtype/network/Address.hpp"
#include "rtype/network/TcpConnection.hpp"
#include "rtype/network/Tls.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <system_error>
#include <vector>

namespace rtype::network
{

/// A TCP connection encrypted with TLS 1.3: the same stream of bytes as a TcpConnection, which nobody on the
/// path can read or change, and on which the client is sure to talk to our server
/// (docs/decisions/0014-encryption.md).
///
/// It is made from a TCP connection not started yet, on each side:
/// @code
/// // Client: the server must present the key whose fingerprint is pinned
/// auto tls = rtype::network::TlsConnection::client(std::move(connection), pinned);
/// // Server: presents its identity
/// auto tls = rtype::network::TlsConnection::server(std::move(connection), identity);
///
/// tls.start(
///     [](std::span<const std::byte> bytes) { /* bytes received, decrypted */ },
///     [](std::error_code reason) { /* closed, lost, or the handshake failed */ });
/// tls.send(hello);
/// @endcode
///
/// start() begins the **handshake**, in which both sides agree on keys and the server proves its identity.
/// The bytes given to send() before its end wait for it, and are sent encrypted once it is done. If it fails,
/// the close handler receives the reason:
/// - NetworkError::UntrustedServer, on the client: the server does not have the pinned key;
/// - NetworkError::HandshakeInterrupted: the TCP connection ended during the handshake, closed or reset by
/// the
///   peer;
/// - an error of tlsCategory(): the peer does not speak TLS 1.3, or sent an invalid message.
///
/// Every function can be called from any thread. The handlers are called on the loop's thread.
class TlsConnection
{
public:
    /// Called with the bytes received and decrypted, on the loop's thread. The data is only valid during the
    /// call.
    using ReceiveHandler = TcpConnection::ReceiveHandler;
    /// Called once when the connection ends without close() being called: closed by the peer (no error),
    /// lost (the system error), or ended by the TLS layer (a NetworkError, or an error of tlsCategory()).
    using CloseHandler = TcpConnection::CloseHandler;

    struct Impl;

private:
    std::shared_ptr<Impl> _impl;
    Address _localAddress;
    Address _remoteAddress;

    explicit TlsConnection(std::shared_ptr<Impl> impl);

public:
    /// The client side of a connection: the handshake fails unless the server presents the key of this
    /// fingerprint.
    [[nodiscard]] static TlsConnection client(TcpConnection connection, const KeyFingerprint &server);
    /// The server side of a connection, which proves it is the server with this identity.
    [[nodiscard]] static TlsConnection server(TcpConnection connection, const ServerIdentity &identity);

    /// Closes the connection, as close() does.
    ~TlsConnection();

    TlsConnection(const TlsConnection &) = delete;
    TlsConnection &operator=(const TlsConnection &) = delete;
    TlsConnection(TlsConnection &&) noexcept;
    TlsConnection &operator=(TlsConnection &&) noexcept;

    /// Address of this end of the connection.
    [[nodiscard]] const Address &localAddress() const { return _localAddress; }
    /// Address of the peer.
    [[nodiscard]] const Address &remoteAddress() const { return _remoteAddress; }

    /// Starts the handshake, then receiving: the receive handler is called with the bytes as they arrive, and
    /// the close handler when the connection ends. Call it once.
    void start(ReceiveHandler onReceive, CloseHandler onClose);

    /// Sends bytes, after those of the previous calls. The data is copied, so it can be reused at once.
    void send(std::span<const std::byte> bytes);
    /// Sends bytes already in a buffer of their own.
    void send(std::vector<std::byte> &&bytes);

    /// Tells the peer that the connection ends, then closes it once the bytes already given to send() are
    /// sent. The handlers are no longer called.
    void close();
};

} // namespace rtype::network
