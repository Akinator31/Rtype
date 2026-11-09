#include "rtype/network/DatagramBody.hpp"

#include "NetworkLogger.hpp"
#include "rtype/network/Reader.hpp"
#include "rtype/network/Writer.hpp"

#include <format>
#include <stdexcept>

namespace rtype::network
{

using detail::logger;

namespace
{

constexpr std::uint8_t ReliableFlag = 0x01;

} // namespace

std::vector<std::byte> encodeBody(const DatagramBody &body)
{
    if (body.messages.size() > MaxBodyMessages) {
        throw std::length_error(
            std::format("a body holds at most {} messages, not {}", MaxBodyMessages, body.messages.size()));
    }
    std::size_t size = BodyHeaderSize;
    for (const BodyMessage &message : body.messages) {
        size += encodedMessageSize(message.reliable, message.payload.size());
    }
    if (size > MaxDatagramBodySize) {
        throw std::length_error(std::format("a body is at most {} bytes, not {}", MaxDatagramBodySize, size));
    }

    Writer writer(size);
    writer.writeU64(body.ack);
    writer.writeU32(body.ackBits);
    writer.writeU16(body.ackDelay);
    writer.writeU8(static_cast<std::uint8_t>(body.messages.size()));
    for (const BodyMessage &message : body.messages) {
        writer.writeU8(message.type);
        writer.writeU8(message.reliable ? ReliableFlag : 0);
        if (message.reliable) {
            writer.writeU16(message.id);
        }
        // Below MaxDatagramBodySize, checked above
        writer.writeU16(static_cast<std::uint16_t>(message.payload.size()));
        writer.writeBytes(message.payload);
    }
    return writer.take();
}

std::optional<DatagramBody> decodeBody(std::span<const std::byte> bytes)
{
    Reader reader(bytes);
    DatagramBody body;
    body.ack = reader.readU64();
    body.ackBits = reader.readU32();
    body.ackDelay = reader.readU16();
    const std::uint8_t count = reader.readU8();
    body.messages.reserve(count);
    for (std::uint8_t i = 0; i < count && reader.ok(); ++i) {
        BodyMessage message;
        message.type = reader.readU8();
        const std::uint8_t flags = reader.readU8();
        if ((flags & ~ReliableFlag) != 0) {
            reader.fail();
            break;
        }
        message.reliable = (flags & ReliableFlag) != 0;
        if (message.reliable) {
            message.id = reader.readU16();
        }
        const std::uint16_t size = reader.readU16();
        message.payload = reader.readBytes(size);
        body.messages.push_back(message);
    }
    if (!reader.finish()) {
        if (logger().isEnabled(rtype::log::Level::Trace)) {
            logger().trace("Datagram body of {} bytes dropped: malformed", bytes.size());
        }
        return std::nullopt;
    }
    return body;
}

} // namespace rtype::network
