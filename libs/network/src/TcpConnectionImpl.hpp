#pragma once

// Private to libs/network: the state of a TCP connection, shared with TcpListener, which creates connections.

#include "Asio.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <deque>
#include <vector>

namespace rtype::network
{

// Owned by the connection and by its pending operations, like UdpSocket::Impl. Only used on the loop's
// thread.
struct TcpConnection::Impl : std::enable_shared_from_this<Impl> {
    static constexpr std::size_t ReadBufferSize = 16384;

    asio::ip::tcp::socket socket;
    std::vector<std::byte> buffer = std::vector<std::byte>(ReadBufferSize);
    /// Data waiting to be written, the first one being written.
    std::deque<std::vector<std::byte>> writes;
    ReceiveHandler onReceive;
    CloseHandler onClose;
    Address local;
    Address remote;
    /// close() was called: no more sends, and the socket closes once the writes are done.
    bool closing = false;
    /// The socket is closed.
    bool closed = false;

    explicit Impl(asio::ip::tcp::socket connected);

    void start(ReceiveHandler receive, CloseHandler close);
    void send(std::vector<std::byte> bytes);
    void requestClose();

private:
    void readNext();
    void onRead(const std::error_code &error, std::size_t size);
    void writeNext();
    void onWritten(const std::error_code &error);
    void shutdown();
    void fail(const std::error_code &error);
};

} // namespace rtype::network
