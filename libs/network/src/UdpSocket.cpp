#include "rtype/network/UdpSocket.hpp"

#include "Asio.hpp"

#include <atomic>
#include <format>
#include <system_error>
#include <vector>

namespace rtype::network
{

using asio::ip::udp;
using detail::logger;

// Owned by the socket and by its pending operations: a datagram arriving after the socket is destroyed finds
// the Impl still alive, and sees that it is closed.
struct UdpSocket::Impl : std::enable_shared_from_this<Impl> {
    udp::socket socket;
    /// Set by close() at once, while the socket itself is closed later on the loop's thread: a datagram
    /// already received then, whose completion waits in the loop before the close (on Windows, in the
    /// completion port), must not reach the handler, whose owner may be gone.
    std::atomic<bool> closed{false};
    udp::endpoint sender;
    std::vector<std::byte> buffer = std::vector<std::byte>(MaxDatagramSize);
    ReceiveHandler handler;
    Address local;

    explicit Impl(asio::io_context &context) : socket(context) {}

    void receiveNext()
    {
        socket.async_receive_from(
            asio::buffer(buffer), sender,
            [self = shared_from_this()](const std::error_code &error, std::size_t size) {
                self->onReceived(error, size);
            });
    }

    void onReceived(const std::error_code &error, std::size_t size)
    {
        if (closed.load(std::memory_order_acquire) || !socket.is_open()) {
            return;
        }
        if (!error) {
            if (logger().isEnabled(rtype::log::Level::Trace)) {
                logger().trace("UDP {} received {} bytes from {}", local, size, detail::fromEndpoint(sender));
            }
            handler(detail::fromEndpoint(sender), std::span<const std::byte>(buffer.data(), size));
        } else if (error != asio::error::operation_aborted) {
            // For instance on Windows, after a datagram sent to a closed port: the socket keeps working
            logger().debug("UDP {} failed to receive: {}", local, error.message());
        }
        if (socket.is_open()) {
            receiveNext();
        }
    }

    void sendNow(const udp::endpoint &to, const std::vector<std::byte> &datagram)
    {
        if (!socket.is_open()) {
            return;
        }
        std::error_code error;
        // Non-blocking: a full buffer drops the datagram, as the network could
        socket.send_to(asio::buffer(datagram), to, 0, error);
        if (error) {
            logger().debug("UDP {} dropped a datagram of {} bytes to {}: {}", local, datagram.size(),
                           detail::fromEndpoint(to), error.message());
        }
    }

    void closeNow()
    {
        if (!socket.is_open()) {
            return;
        }
        std::error_code error;
        socket.close(error);
        handler = nullptr;
        logger().debug("UDP {} closed", local);
    }
};

UdpSocket::UdpSocket(EventLoop &loop, const Address &local)
    : _impl(std::make_shared<Impl>(detail::EventLoopAccess::context(loop)))
{
    const auto endpoint = detail::toEndpoint<udp::endpoint>(local);
    std::error_code error;
    _impl->socket.open(endpoint.protocol(), error);
    if (!error) {
        _impl->socket.bind(endpoint, error);
    }
    if (!error) {
        _impl->socket.non_blocking(true, error);
    }
    if (error) {
        throw std::system_error(error, std::format("cannot open a UDP socket on {}", local));
    }
    _localAddress = detail::fromEndpoint(_impl->socket.local_endpoint());
    _impl->local = _localAddress;
    logger().debug("UDP {} open", _localAddress);
}

UdpSocket::~UdpSocket()
{
    close();
}

UdpSocket::UdpSocket(UdpSocket &&) noexcept = default;

UdpSocket &UdpSocket::operator=(UdpSocket &&other) noexcept
{
    if (this != &other) {
        close();
        _impl = std::move(other._impl);
        _localAddress = other._localAddress;
    }
    return *this;
}

void UdpSocket::startReceiving(ReceiveHandler handler)
{
    if (!_impl) {
        return;
    }
    asio::post(_impl->socket.get_executor(), [impl = _impl, handler = std::move(handler)]() mutable {
        if (!impl->closed.load(std::memory_order_acquire) && impl->socket.is_open()) {
            impl->handler = std::move(handler);
            impl->receiveNext();
        }
    });
}

void UdpSocket::send(const Address &to, std::span<const std::byte> datagram)
{
    if (!_impl) {
        return;
    }
    asio::post(
        _impl->socket.get_executor(),
        [impl = _impl, endpoint = detail::toEndpoint<udp::endpoint>(to),
         data = std::vector<std::byte>(datagram.begin(), datagram.end())] { impl->sendNow(endpoint, data); });
}

void UdpSocket::close()
{
    if (!_impl) {
        return;
    }
    // The handler is no longer called from now on; the datagrams given to send() before still go out
    _impl->closed.store(true, std::memory_order_release);
    asio::post(_impl->socket.get_executor(), [impl = _impl] { impl->closeNow(); });
}

} // namespace rtype::network
