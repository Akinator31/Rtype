#include "rtype/network/Writer.hpp"

#include "rtype/network/Text.hpp"

#include <format>
#include <stdexcept>

namespace rtype::network
{

namespace
{

// Appends the `size` low bytes of a value, most significant first (big-endian)
void appendBigEndian(std::vector<std::byte> &bytes, std::uint64_t value, std::size_t size)
{
    for (std::size_t i = size; i > 0; --i) {
        bytes.push_back(static_cast<std::byte>((value >> (8 * (i - 1))) & 0xFF));
    }
}

} // namespace

Writer::Writer(std::size_t capacity)
{
    _bytes.reserve(capacity);
}

void Writer::writeU8(std::uint8_t value)
{
    _bytes.push_back(static_cast<std::byte>(value));
}

void Writer::writeU16(std::uint16_t value)
{
    appendBigEndian(_bytes, value, 2);
}

void Writer::writeU32(std::uint32_t value)
{
    appendBigEndian(_bytes, value, 4);
}

void Writer::writeU64(std::uint64_t value)
{
    appendBigEndian(_bytes, value, 8);
}

void Writer::writeI16(std::int16_t value)
{
    // Two's complement, which C++20 guarantees for signed integers
    writeU16(static_cast<std::uint16_t>(value));
}

void Writer::writeI32(std::int32_t value)
{
    writeU32(static_cast<std::uint32_t>(value));
}

void Writer::writeBool(bool value)
{
    writeU8(value ? 1 : 0);
}

void Writer::writeBytes(std::span<const std::byte> bytes)
{
    _bytes.insert(_bytes.end(), bytes.begin(), bytes.end());
}

void Writer::writeString(std::string_view text)
{
    if (text.size() > 255) {
        throw std::length_error(std::format("a str8 field holds at most 255 bytes, not {}", text.size()));
    }
    if (!isValidText(text)) {
        throw std::invalid_argument("a str8 field must be valid UTF-8 without control characters");
    }
    writeU8(static_cast<std::uint8_t>(text.size()));
    for (const char c : text) {
        _bytes.push_back(static_cast<std::byte>(c));
    }
}

void Writer::writeString16(std::string_view text)
{
    if (text.size() > 65535) {
        throw std::length_error(std::format("a str16 field holds at most 65535 bytes, not {}", text.size()));
    }
    if (!isValidText(text)) {
        throw std::invalid_argument("a str16 field must be valid UTF-8 without control characters");
    }
    writeU16(static_cast<std::uint16_t>(text.size()));
    for (const char c : text) {
        _bytes.push_back(static_cast<std::byte>(c));
    }
}

void Writer::patchU16(std::size_t offset, std::uint16_t value)
{
    if (offset > _bytes.size() || _bytes.size() - offset < 2) {
        throw std::out_of_range(
            std::format("cannot patch 2 bytes at offset {} of {}", offset, _bytes.size()));
    }
    _bytes[offset] = static_cast<std::byte>(value >> 8);
    _bytes[offset + 1] = static_cast<std::byte>(value & 0xFF);
}

std::vector<std::byte> Writer::take()
{
    std::vector<std::byte> bytes = std::move(_bytes);
    _bytes.clear();
    return bytes;
}

} // namespace rtype::network
