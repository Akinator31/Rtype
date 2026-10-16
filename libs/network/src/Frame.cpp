#include "rtype/network/Frame.hpp"

#include <format>
#include <stdexcept>

namespace rtype::network
{

std::vector<std::byte> makeFrame(std::uint8_t type, std::span<const std::byte> payload)
{
    if (payload.size() > MaxFramePayloadSize) {
        throw std::length_error(std::format("a frame payload holds at most {} bytes, not {}",
                                            MaxFramePayloadSize, payload.size()));
    }
    const auto size = static_cast<std::uint16_t>(payload.size() + 1);
    std::vector<std::byte> frame;
    frame.reserve(payload.size() + 3);
    frame.push_back(static_cast<std::byte>(size >> 8));
    frame.push_back(static_cast<std::byte>(size & 0xFF));
    frame.push_back(static_cast<std::byte>(type));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

void FrameDecoder::feed(std::span<const std::byte> bytes)
{
    if (failed()) {
        return;
    }
    // Drops the bytes of the frames already given, so that the buffer only holds the frame being received
    // and the new bytes
    if (_start > 0) {
        _buffer.erase(_buffer.begin(), _buffer.begin() + static_cast<std::ptrdiff_t>(_start));
        _start = 0;
    }
    _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
}

std::optional<Frame> FrameDecoder::next()
{
    if (failed() || buffered() < 2) {
        return std::nullopt;
    }
    const std::size_t size =
        (static_cast<std::size_t>(_buffer[_start]) << 8) | static_cast<std::size_t>(_buffer[_start + 1]);
    // Checked as soon as the size is known, before waiting for the rest of the frame
    if (size == 0) {
        _error = NetworkError::EmptyFrame;
    } else if (size > MaxFrameSize - 2) {
        _error = NetworkError::FrameTooLarge;
    }
    if (failed()) {
        _buffer.clear();
        _start = 0;
        return std::nullopt;
    }
    if (buffered() < 2 + size) {
        return std::nullopt;
    }
    Frame frame;
    frame.type = static_cast<std::uint8_t>(_buffer[_start + 2]);
    frame.payload = std::span<const std::byte>(_buffer).subspan(_start + 3, size - 1);
    _start += 2 + size;
    return frame;
}

} // namespace rtype::network
