#pragma once

#include "rtype/network/Errors.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

namespace rtype::network
{

/// Largest TCP frame, size field included (docs/protocol/lobby.md, section 3).
inline constexpr std::size_t MaxFrameSize = 16384;
/// Largest payload of a frame: the frame without its size field (2 bytes) and its type (1 byte).
inline constexpr std::size_t MaxFramePayloadSize = MaxFrameSize - 3;

/// One message received on a TCP stream: its type and its payload.
struct Frame {
    std::uint8_t type = 0;
    /// Points into the decoder that produced the frame: valid until the decoder is used again.
    std::span<const std::byte> payload;
};

/// Builds the bytes of a frame: its size on two bytes (type and payload), its type, then its payload.
/// @throws std::length_error if the payload is larger than MaxFramePayloadSize.
[[nodiscard]] std::vector<std::byte> makeFrame(std::uint8_t type, std::span<const std::byte> payload);

/// Splits a TCP stream into frames.
///
/// TCP delivers bytes in arbitrary pieces: a frame may arrive in several pieces, and several frames in one.
/// The decoder keeps the bytes of the frame being received, and gives the frames as soon as they are
/// complete:
/// @code
/// decoder.feed(bytes);
/// while (const auto frame = decoder.next()) {
///     handle(frame->type, frame->payload);
/// }
/// if (decoder.failed()) {
///     connection.close(); // the peer does not follow the protocol
/// }
/// @endcode
///
/// A size of 0, or above the maximum, makes the decoder fail as soon as the two bytes of the size are there,
/// without waiting for the rest: a peer cannot make it keep more than one frame in memory. Once failed, the
/// decoder ignores everything.
///
/// The decoder only works on the bytes it is given, with no socket: it can be tested and fuzzed alone.
class FrameDecoder
{
private:
    std::vector<std::byte> _buffer;
    /// Start of the bytes not consumed yet, in _buffer.
    std::size_t _start = 0;
    std::error_code _error;

public:
    /// Adds bytes received on the stream. Invalidates the payload of the frames given before.
    void feed(std::span<const std::byte> bytes);

    /// The next complete frame, or nothing if it is not complete yet or if the decoder failed. Invalidates
    /// the payload of the frame given before.
    [[nodiscard]] std::optional<Frame> next();

    /// Tells whether the stream broke the framing rules. The stream must then be closed.
    [[nodiscard]] bool failed() const { return static_cast<bool>(_error); }
    /// Why the decoder failed: NetworkError::EmptyFrame or NetworkError::FrameTooLarge.
    [[nodiscard]] std::error_code error() const { return _error; }

    /// Number of bytes kept, waiting for the end of a frame.
    [[nodiscard]] std::size_t buffered() const { return _buffer.size() - _start; }
};

} // namespace rtype::network
