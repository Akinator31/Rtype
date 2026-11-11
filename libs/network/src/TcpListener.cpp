#include "rtype/network/TcpListener.hpp"

#include "Asio.hpp"
#include "TcpConnectionImpl.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <system_error>

namespace rtype::network
{

using asio::ip::tcp;
using detail::logger;

// Owned by the listener and by its pending operations, like UdpSocket::Impl.
struct TcpListener::Impl : std::enable_shared_from_this<Impl> {
    /// Delay before accepting again after a failure, for instance when the process has no file descriptor
    /// left.
    static constexpr std::chrono::milliseconds RetryDelay{100};

    tcp::acceptor acceptor;
    asio::steady_timer retry;
    /// Set by close() at once, as UdpSocket::Impl::closed: a connection accepted before the acceptor is
    /// closed on the loop's thread is not delivered.
    std::atomic<bool> closed{false};
    /// Wraps the accept handler: TcpConnection's constructor is only reachable from TcpListener.
    std::function<void(std::shared_ptr<TcpConnection::Impl>)> deliver;
    Address local;

    explicit Impl(asio::io_context &context) : acceptor(context), retry(context) {}

    void acceptNext()
    {
        acceptor.async_accept([self = shared_from_this()](const std::error_code &error, tcp::socket socket) {
            self->onAccepted(error, std::move(socket));
        });
    }

    void onAccepted(const std::error_code &error, tcp::socket socket)
    {
        if (closed.load(std::memory_order_acquire) || !acceptor.is_open()) {
            return;
        }
        if (!error) {
            auto connection = std::make_shared<TcpConnection::Impl>(std::move(socket));
            logger().debug("TCP {} accepted a connection from {}", local, connection->remote);
            deliver(std::move(connection));
            if (acceptor.is_open()) {
                acceptNext();
            }
            return;
        }
        if (error == asio::error::operation_aborted) {
            return;
        }
        logger().warning("TCP {} failed to accept a connection: {}", local, error.message());
        retry.expires_after(RetryDelay);
        retry.async_wait([self = shared_from_this()](const std::error_code &waitError) {
            if (!waitError && self->acceptor.is_open()) {
                self->acceptNext();
            }
        });
    }

    void closeNow()
    {
        if (!acceptor.is_open()) {
            return;
        }
        std::error_code error;
        acceptor.close(error);
        retry.cancel();
        deliver = nullptr;
        logger().debug("TCP {} stopped listening", local);
    }
};

TcpListener::TcpListener(EventLoop &loop, const Address &local)
    : _impl(std::make_shared<Impl>(detail::EventLoopAccess::context(loop)))
{
    const auto endpoint = detail::toEndpoint<tcp::endpoint>(local);
    tcp::acceptor &acceptor = _impl->acceptor;
    std::error_code error;
    acceptor.open(endpoint.protocol(), error);
#ifndef _WIN32
    // Lets a restarted server listen again at once on its port. Not on Windows, where this option would let
    // another process take a port in use.
    if (!error) {
        acceptor.set_option(tcp::acceptor::reuse_address(true), error);
    }
#endif
    if (!error) {
        acceptor.bind(endpoint, error);
    }
    if (!error) {
        acceptor.listen(asio::socket_base::max_listen_connections, error);
    }
    if (error) {
        throw std::system_error(error, std::format("cannot listen on TCP {}", local));
    }
    _localAddress = detail::fromEndpoint(acceptor.local_endpoint());
    _impl->local = _localAddress;
    logger().debug("TCP {} listening", _localAddress);
}

TcpListener::~TcpListener()
{
    close();
}

TcpListener::TcpListener(TcpListener &&) noexcept = default;

TcpListener &TcpListener::operator=(TcpListener &&other) noexcept
{
    if (this != &other) {
        close();
        _impl = std::move(other._impl);
        _localAddress = other._localAddress;
    }
    return *this;
}

void TcpListener::startAccepting(AcceptHandler handler)
{
    if (!_impl) {
        return;
    }
    auto deliver = [handler = std::move(handler)](std::shared_ptr<TcpConnection::Impl> connection) {
        handler(TcpConnection(std::move(connection)));
    };
    asio::post(_impl->acceptor.get_executor(), [impl = _impl, deliver = std::move(deliver)]() mutable {
        if (!impl->closed.load(std::memory_order_acquire) && impl->acceptor.is_open()) {
            impl->deliver = std::move(deliver);
            impl->acceptNext();
        }
    });
}

void TcpListener::close()
{
    if (!_impl) {
        return;
    }
    _impl->closed.store(true, std::memory_order_release);
    asio::post(_impl->acceptor.get_executor(), [impl = _impl] { impl->closeNow(); });
}

} // namespace rtype::network
