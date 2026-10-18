#include "rtype/network/TlsConnection.hpp"

#include "Asio.hpp"
#include "TcpConnectionImpl.hpp"
#include "Tls.hpp"
#include "rtype/network/Errors.hpp"

#include <atomic>

namespace rtype::network
{

using detail::logger;

// Owned by the connection and by the functions posted to the loop; the handlers of the TCP connection only
// keep a weak reference. Used on the loop's thread only, except `closed`.
struct TlsConnection::Impl : std::enable_shared_from_this<Impl> {
    std::shared_ptr<TcpConnection::Impl> tcp;
    detail::TlsSession session;
    ReceiveHandler onReceive;
    CloseHandler onClose;
    SecuredHandler onSecured;
    /// close() was called, or the connection ended: no handler is called any more. Set from any thread.
    std::atomic<bool> closed = false;
    /// start() was called on the loop's thread.
    bool started = false;
    /// close() was run on the loop's thread: what is given to send() after it is dropped.
    bool closeCalled = false;
    /// close() waits for the end of the handshake, to send the bytes given to send() before it.
    bool closing = false;

    /// `side` is the pinned key of the server (client), or the identity of the server (server).
    template <typename Side>
    Impl(std::shared_ptr<TcpConnection::Impl> connection, Side &&side)
        : tcp(std::move(connection)), session(std::forward<Side>(side))
    {
    }

    template <typename Function>
    void post(Function function)
    {
        asio::post(tcp->socket.get_executor(), std::move(function));
    }

    void start()
    {
        // Even if close() was called since: the bytes given to send() before it are sent once secured
        started = true;
        const std::weak_ptr<Impl> weak = weak_from_this();
        tcp->start(
            [weak](std::span<const std::byte> bytes) {
                if (const auto self = weak.lock(); self && (!self->closed || self->closing)) {
                    self->receive(bytes);
                }
            },
            [weak](std::error_code reason) {
                if (const auto self = weak.lock()) {
                    self->tcpClosed(reason);
                }
            });
        logger().debug("TLS {} handshake started", tcp->remote);
        session.start();
        flush();
        checkSession(false);
    }

    void receive(std::span<const std::byte> bytes)
    {
        const bool wasSecured = session.secured();
        const std::vector<std::byte> decrypted = session.receive(bytes);
        flush();
        if (!wasSecured && session.secured()) {
            logger().debug("TLS {} secured with {}", tcp->remote, session.cipherSuite());
            if (!closed && onSecured) {
                // Once: the session stays secured
                const SecuredHandler handler = std::move(onSecured);
                onSecured = nullptr;
                handler();
            }
        }
        if (closing) {
            // close() was waiting for the handshake to send the bytes given before it: they are sent now
            if (session.secured()) {
                finishClose();
            } else if (session.error()) {
                closing = false;
                tcp->requestClose();
            }
            return;
        }
        if (!decrypted.empty() && !closed && onReceive) {
            if (logger().isEnabled(rtype::log::Level::Trace)) {
                logger().trace("TLS {} decrypted {} bytes", tcp->remote, decrypted.size());
            }
            onReceive(decrypted);
        }
        checkSession(true);
    }

    /// Ends the connection if the session failed or the peer closed it.
    void checkSession(bool afterReceive)
    {
        if (closed) {
            return;
        }
        if (const std::error_code error = session.error()) {
            if (error == NetworkError::UntrustedServer) {
                logger().error("TLS server {} presented the key {}, not the pinned one: it is not our server",
                               tcp->remote,
                               session.presentedKey() ? session.presentedKey()->toHex() : "none");
            } else {
                logger().warning("TLS {} failed{}: {}", tcp->remote,
                                 session.secured() ? "" : " during the handshake", error.message());
            }
            // Sends the alert telling the peer why, if Mbed TLS made one
            tcp->requestClose();
            end(error);
        } else if (afterReceive && session.peerClosed()) {
            logger().debug("TLS {} closed by the peer", tcp->remote);
            tcp->requestClose();
            end({});
        }
    }

    void tcpClosed(std::error_code reason)
    {
        // The peer closing its socket before reading what was sent to it resets the connection instead of
        // ending it normally (always on Windows, where the ClientHello is often still unread)
        const bool closedByPeer =
            !reason || reason == asio::error::connection_reset || reason == asio::error::connection_aborted;
        if (closedByPeer && !session.secured()) {
            if (reason) {
                logger().warning("TLS {} closed during the handshake: {}", tcp->remote, reason.message());
            } else {
                logger().warning("TLS {} closed during the handshake", tcp->remote);
            }
            reason = NetworkError::HandshakeInterrupted;
        }
        end(reason);
    }

    void send(std::vector<std::byte> bytes)
    {
        // Not `closed`, set as soon as close() is called: the bytes given to send() before it are still sent
        if (closeCalled || bytes.empty()) {
            return;
        }
        session.write(bytes);
        flush();
        checkSession(false);
    }

    void close()
    {
        closeCalled = true;
        if (started && !session.secured() && !session.error() && session.hasPending()) {
            closing = true;
            return;
        }
        finishClose();
    }

    void finishClose()
    {
        closing = false;
        session.closeNotify();
        flush();
        tcp->requestClose();
    }

    /// Sends the encrypted bytes the session made.
    void flush()
    {
        std::vector<std::byte> output = session.takeOutput();
        if (!output.empty()) {
            tcp->send(std::move(output));
        }
    }

    void end(std::error_code reason)
    {
        if (closed.exchange(true)) {
            return;
        }
        onReceive = nullptr;
        const CloseHandler handler = std::move(onClose);
        onClose = nullptr;
        if (handler) {
            handler(reason);
        }
    }
};

TlsConnection::TlsConnection(std::shared_ptr<Impl> impl)
    : _impl(std::move(impl)), _localAddress(_impl->tcp->local), _remoteAddress(_impl->tcp->remote)
{
}

TlsConnection TlsConnection::client(TcpConnection connection, const KeyFingerprint &server)
{
    return TlsConnection(std::make_shared<Impl>(std::move(connection._impl), server));
}

TlsConnection TlsConnection::server(TcpConnection connection, const ServerIdentity &identity)
{
    return TlsConnection(std::make_shared<Impl>(std::move(connection._impl), identity._impl));
}

TlsConnection::~TlsConnection()
{
    close();
}

TlsConnection::TlsConnection(TlsConnection &&) noexcept = default;

TlsConnection &TlsConnection::operator=(TlsConnection &&other) noexcept
{
    if (this != &other) {
        close();
        _impl = std::move(other._impl);
        _localAddress = other._localAddress;
        _remoteAddress = other._remoteAddress;
    }
    return *this;
}

void TlsConnection::start(ReceiveHandler onReceive, CloseHandler onClose, SecuredHandler onSecured)
{
    if (!_impl) {
        return;
    }
    _impl->post([impl = _impl, onReceive = std::move(onReceive), onClose = std::move(onClose),
                 onSecured = std::move(onSecured)]() mutable {
        impl->onReceive = std::move(onReceive);
        impl->onClose = std::move(onClose);
        impl->onSecured = std::move(onSecured);
        impl->start();
    });
}

void TlsConnection::send(std::span<const std::byte> bytes)
{
    send(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

void TlsConnection::send(std::vector<std::byte> &&bytes)
{
    if (!_impl) {
        return;
    }
    _impl->post([impl = _impl, data = std::move(bytes)]() mutable { impl->send(std::move(data)); });
}

void TlsConnection::close()
{
    if (!_impl) {
        return;
    }
    _impl->closed = true;
    _impl->post([impl = _impl] { impl->close(); });
}

} // namespace rtype::network
