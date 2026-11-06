#include "Instance/DirectTransport.hpp"

#include <utility>

namespace rtype::server
{

DirectTransport::DirectTransport(rtype::network::EventLoop &loop, const rtype::network::Address &local)
    : _socket(loop, local)
{
}

void DirectTransport::start(ReceiveHandler handler)
{
    _socket.startReceiving(std::move(handler));
}

void DirectTransport::send(const rtype::network::Address &to, std::span<const std::byte> datagram)
{
    _socket.send(to, datagram);
}

void DirectTransport::close()
{
    _socket.close();
}

const rtype::network::Address &DirectTransport::localAddress() const
{
    return _socket.localAddress();
}

} // namespace rtype::server
