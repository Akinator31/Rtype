#include "Instance/RelayedTransport.hpp"

#include "Common/RelayFrame.hpp"
#include "rtype/log/Logger.hpp"

#include <stdexcept>
#include <utility>

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("instance");
    return instance;
}

} // namespace

RelayedTransport::RelayedTransport(rtype::network::EventLoop &loop, std::uint16_t relayPort)
    : _socket(loop, rtype::network::Address::loopbackIpv4(0)),
      _relay(rtype::network::Address::loopbackIpv4(relayPort))
{
}

void RelayedTransport::start(ReceiveHandler handler)
{
    _socket.startReceiving([this, handler = std::move(handler)](const rtype::network::Address &from,
                                                                std::span<const std::byte> received) {
        if (from != _relay) {
            logger().trace("Datagram from {} on the loopback dropped: not the relay", from);
            return;
        }
        const std::optional<RelayFrame> frame = decodeRelayFrame(received);
        if (!frame) {
            logger().trace("Malformed relay frame dropped ({} bytes)", received.size());
            return;
        }
        handler(frame->client, frame->datagram);
    });
}

void RelayedTransport::send(const rtype::network::Address &to, std::span<const std::byte> datagram)
{
    try {
        _socket.send(_relay, encodeRelayFrame(to, datagram));
    } catch (const std::invalid_argument &error) {
        logger().debug("Datagram to {} not relayed: {}", to, error.what());
    }
}

void RelayedTransport::close()
{
    _socket.close();
}

const rtype::network::Address &RelayedTransport::localAddress() const
{
    return _socket.localAddress();
}

} // namespace rtype::server
