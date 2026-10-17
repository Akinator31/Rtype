#pragma once

#include "rtype/network/Datagram.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rtype::network
{

/// Size of the fields before the messages of a body: `Ack`, `Ack bits` and `Count`.
inline constexpr std::size_t BodyHeaderSize = 13;
/// Size of the header of an unreliable message: type, flags and size.
inline constexpr std::size_t UnreliableMessageHeaderSize = 4;
/// Size of the header of a reliable message: type, flags, message id and size.
inline constexpr std::size_t ReliableMessageHeaderSize = 6;
/// Largest payload of an unreliable message: alone in a datagram.
inline constexpr std::size_t MaxUnreliablePayloadSize =
    MaxDatagramBodySize - BodyHeaderSize - UnreliableMessageHeaderSize;
/// Largest payload of a reliable message: alone in a datagram.
inline constexpr std::size_t MaxReliablePayloadSize =
    MaxDatagramBodySize - BodyHeaderSize - ReliableMessageHeaderSize;
/// Most messages in one body: its count is a `u8`.
inline constexpr std::size_t MaxBodyMessages = 255;

/// One message of a body (docs/protocol/game.md, section 5).
struct BodyMessage {
    std::uint8_t type = 0;
    /// Resent until acknowledged, and delivered once and in order.
    bool reliable = false;
    /// Only for a reliable message.
    std::uint16_t id = 0;
    /// When decoded, points into the decoded body: copy it to keep it.
    std::span<const std::byte> payload;
};

/// The decrypted body of a datagram: the acknowledgements of the packets received from the peer, and the
/// messages. A body without messages is a heartbeat.
struct DatagramBody {
    /// Highest packet number received from the peer, 0 if none.
    std::uint64_t ack = 0;
    /// Bit i set: packet `ack - 1 - i` was received.
    std::uint32_t ackBits = 0;
    std::vector<BodyMessage> messages;
};

/// Writes a body.
/// @throws std::length_error if it holds more than MaxBodyMessages messages, or does not fit in a datagram
/// (MaxDatagramBodySize).
[[nodiscard]] std::vector<std::byte> encodeBody(const DatagramBody &body);

/// Reads a body, strictly: nothing if a size goes past the end, if bytes are left after the last message, or
/// if a reserved flag is set. The payloads point into `bytes`, which must stay valid while they are used.
[[nodiscard]] std::optional<DatagramBody> decodeBody(std::span<const std::byte> bytes);

/// Size of a message once written in a body, header included.
[[nodiscard]] constexpr std::size_t encodedMessageSize(bool reliable, std::size_t payloadSize)
{
    return (reliable ? ReliableMessageHeaderSize : UnreliableMessageHeaderSize) + payloadSize;
}

} // namespace rtype::network
