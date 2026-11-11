#include "rtype/network/TcpConnection.hpp"

#include "Asio.hpp"
#include "TcpConnectionImpl.hpp"

#include <format>
#include <system_error>
#include <vector>

namespace rtype::network
{

using asio::ip::tcp;
using detail::logger;

TcpConnection::Impl::Impl(tcp::socket connected) : socket(std::move(connected))
{
    std::error_code error;
    socket.set_option(tcp::no_delay(true), error); // small messages are sent at once
    const auto localEndpoint = socket.local_endpoint(error);
    if (!error) {
        local = detail::fromEndpoint(localEndpoint);
    }
    const auto remoteEndpoint = socket.remote_endpoint(error);
    if (!error) {
        remote = detail::fromEndpoint(remoteEndpoint);
    }
}

void TcpConnection::Impl::start(ReceiveHandler receive, CloseHandler close)
{
    if (closed || closing) {
        return;
    }
    onReceive = std::move(receive);
    onClose = std::move(close);
    readNext();
}

void TcpConnection::Impl::readNext()
{
    socket.async_read_some(asio::buffer(buffer),
                           [self = shared_from_this()](const std::error_code &error, std::size_t size) {
                               self->onRead(error, size);
                           });
}

void TcpConnection::Impl::onRead(const std::error_code &error, std::size_t size)
{
    if (closed) {
        return;
    }
    if (error) {
        fail(error);
        return;
    }
    if (logger().isEnabled(rtype::log::Level::Trace)) {
        logger().trace("TCP {} received {} bytes", remote, size);
    }
    if (!closing && !closeRequested.load(std::memory_order_acquire) && onReceive) {
        onReceive(std::span<const std::byte>(buffer.data(), size));
    }
    if (!closed) {
        readNext();
    }
}

void TcpConnection::Impl::send(std::vector<std::byte> bytes)
{
    if (closed || closing || bytes.empty()) {
        return;
    }
    writes.push_back(std::move(bytes));
    if (writes.size() == 1) {
        writeNext();
    }
}

void TcpConnection::Impl::writeNext()
{
    asio::async_write(
        socket, asio::buffer(writes.front()),
        [self = shared_from_this()](const std::error_code &error, std::size_t) { self->onWritten(error); });
}

void TcpConnection::Impl::onWritten(const std::error_code &error)
{
    if (closed) {
        return;
    }
    if (error) {
        fail(error);
        return;
    }
    writes.pop_front();
    if (!writes.empty()) {
        writeNext();
    } else if (closing) {
        shutdown();
    }
}

void TcpConnection::Impl::requestClose()
{
    if (closed || closing) {
        return;
    }
    closing = true;
    onReceive = nullptr;
    onClose = nullptr;
    if (writes.empty()) {
        shutdown();
    }
}

void TcpConnection::Impl::shutdown()
{
    closed = true;
    writes.clear();
    std::error_code error;
    socket.shutdown(tcp::socket::shutdown_both, error);
    socket.close(error);
    logger().debug("TCP {} closed", remote);
}

void TcpConnection::Impl::fail(const std::error_code &error)
{
    closed = true;
    writes.clear();
    std::error_code ignored;
    socket.close(ignored);
    // The end of the stream is a normal close by the peer, not an error
    const std::error_code reason = error == asio::error::eof ? std::error_code() : error;
    if (reason) {
        logger().debug("TCP {} lost: {}", remote, reason.message());
    } else {
        logger().debug("TCP {} closed by the peer", remote);
    }
    onReceive = nullptr;
    const CloseHandler handler = std::move(onClose);
    onClose = nullptr;
    if (handler && !closeRequested.load(std::memory_order_acquire)) {
        handler(reason);
    }
}

TcpConnection::TcpConnection(std::shared_ptr<Impl> impl)
    : _impl(std::move(impl)), _localAddress(_impl->local), _remoteAddress(_impl->remote)
{
}

void TcpConnection::connect(EventLoop &loop, const Address &remote, ConnectHandler handler,
                            std::chrono::milliseconds timeout)
{
    struct Attempt {
        tcp::socket socket;
        asio::steady_timer timer;
        ConnectHandler handler;
        bool timedOut = false;
    };
    asio::io_context &context = detail::EventLoopAccess::context(loop);
    auto attempt =
        std::make_shared<Attempt>(tcp::socket(context), asio::steady_timer(context), std::move(handler));

    asio::post(context, [attempt, remote, timeout] {
        attempt->timer.expires_after(timeout);
        attempt->timer.async_wait([attempt](const std::error_code &error) {
            if (!error) {
                attempt->timedOut = true;
                std::error_code ignored;
                attempt->socket.close(ignored);
            }
        });
        attempt->socket.async_connect(
            detail::toEndpoint<tcp::endpoint>(remote), [attempt, remote](std::error_code error) {
                attempt->timer.cancel();
                if (attempt->timedOut) {
                    error = std::make_error_code(std::errc::timed_out);
                }
                if (error) {
                    logger().debug("TCP connection to {} failed: {}", remote, error.message());
                    attempt->handler(error, std::nullopt);
                    return;
                }
                logger().debug("TCP connected to {}", remote);
                attempt->handler({}, TcpConnection(std::make_shared<Impl>(std::move(attempt->socket))));
            });
    });
}

TcpConnection::~TcpConnection()
{
    close();
}

TcpConnection::TcpConnection(TcpConnection &&) noexcept = default;

TcpConnection &TcpConnection::operator=(TcpConnection &&other) noexcept
{
    if (this != &other) {
        close();
        _impl = std::move(other._impl);
        _localAddress = other._localAddress;
        _remoteAddress = other._remoteAddress;
    }
    return *this;
}

void TcpConnection::start(ReceiveHandler onReceive, CloseHandler onClose)
{
    if (!_impl) {
        return;
    }
    asio::post(_impl->socket.get_executor(),
               [impl = _impl, onReceive = std::move(onReceive), onClose = std::move(onClose)]() mutable {
                   impl->start(std::move(onReceive), std::move(onClose));
               });
}

void TcpConnection::send(std::span<const std::byte> bytes)
{
    send(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

void TcpConnection::send(std::vector<std::byte> &&bytes)
{
    if (!_impl) {
        return;
    }
    asio::post(_impl->socket.get_executor(),
               [impl = _impl, data = std::move(bytes)]() mutable { impl->send(std::move(data)); });
}

void TcpConnection::close()
{
    if (!_impl) {
        return;
    }
    // The handlers are no longer called from now on; the bytes given to send() before still go out
    _impl->closeRequested.store(true, std::memory_order_release);
    asio::post(_impl->socket.get_executor(), [impl = _impl] { impl->requestClose(); });
}

} // namespace rtype::network
