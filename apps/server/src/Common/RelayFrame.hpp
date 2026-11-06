#pragma once

#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

/// The relay frame: a datagram of the game with the address of its client, between the relay of the master
/// and an instance, on the loopback, in both directions (docs/protocol/instances.md, section 5).
///
/// | Offset | Field    | Type     |                                                       |
/// |--------|----------|----------|-------------------------------------------------------|
/// | 0      | Family   | u8       | 4 or 6                                                |
/// | 1      | Address  | 16 bytes | IPv4 in the first 4 bytes, the rest zero; or IPv6     |
/// | 17     | Port     | u16      | port of the client, big endian, never 0               |
/// | 19     | Datagram | rest     | the datagram of the game, unchanged, 1 to 1200 bytes  |
///
/// Pure, with no socket, as the datagrams: it can be tested and fuzzed alone.
/// @code
/// socket.send(relay, rtype::server::encodeRelayFrame(client, datagram));
/// ...
/// const auto frame = rtype::server::decodeRelayFrame(received);
/// if (!frame) return; // dropped
/// handle(frame->client, frame->datagram);
/// @endcode
namespace rtype::server
{

/// Size of the fields before the datagram.
inline constexpr std::size_t RelayFrameHeaderSize = 19;
/// Largest frame: its header and the largest datagram of the game.
inline constexpr std::size_t MaxRelayFrameSize = RelayFrameHeaderSize + rtype::network::MaxSealedDatagramSize;

/// A decoded frame.
struct RelayFrame {
    /// The client the datagram comes from, or goes to.
    rtype::network::Address client;
    /// Points into the decoded bytes: copy it to keep it.
    std::span<const std::byte> datagram;
};

/// Writes a frame.
/// @throws std::invalid_argument if the port of the client is 0, or the datagram empty or larger than
/// MaxSealedDatagramSize.
[[nodiscard]] std::vector<std::byte> encodeRelayFrame(const rtype::network::Address &client,
                                                      std::span<const std::byte> datagram);

/// Reads a frame, strictly: nothing if it is shorter than its header, if its family is not 4 or 6, if the
/// tail of an IPv4 address is not zero, if its port is 0, or if its datagram is empty or larger than
/// MaxSealedDatagramSize. The datagram itself is not checked: the session that receives it does.
[[nodiscard]] std::optional<RelayFrame> decodeRelayFrame(std::span<const std::byte> frame);

} // namespace rtype::server
