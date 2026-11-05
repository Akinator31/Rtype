#include "rtype/network/Reader.hpp"

#include "rtype/network/Text.hpp"

#include <algorithm>

namespace rtype::network
{

std::string_view toString(ReadError error)
{
    switch (error) {
    case ReadError::None:
        return "no error";
    case ReadError::TooShort:
        return "too short";
    case ReadError::InvalidBool:
        return "invalid bool";
    case ReadError::InvalidLength:
        return "length out of range";
    case ReadError::InvalidText:
        return "invalid text";
    case ReadError::TrailingBytes:
        return "trailing bytes";
    case ReadError::InvalidValue:
        return "invalid value";
    }
    return "unknown error";
}

Reader::Reader(std::span<const std::byte> data) : _data(data) {}

std::span<const std::byte> Reader::take(std::size_t size)
{
    if (!ok()) {
        return {};
    }
    if (size > remaining()) {
        _error = ReadError::TooShort;
        return {};
    }
    const std::span<const std::byte> bytes = _data.subspan(_position, size);
    _position += size;
    return bytes;
}

std::uint64_t Reader::readUnsigned(std::size_t size)
{
    std::uint64_t value = 0;
    for (const std::byte byte : take(size)) {
        value = (value << 8) | static_cast<std::uint64_t>(byte);
    }
    return value;
}

std::uint8_t Reader::readU8()
{
    return static_cast<std::uint8_t>(readUnsigned(1));
}

std::uint16_t Reader::readU16()
{
    return static_cast<std::uint16_t>(readUnsigned(2));
}

std::uint32_t Reader::readU32()
{
    return static_cast<std::uint32_t>(readUnsigned(4));
}

std::uint64_t Reader::readU64()
{
    return readUnsigned(8);
}

std::int16_t Reader::readI16()
{
    // Two's complement, which C++20 guarantees for signed integers
    return static_cast<std::int16_t>(readU16());
}

std::int32_t Reader::readI32()
{
    return static_cast<std::int32_t>(readU32());
}

bool Reader::readBool()
{
    const std::uint8_t value = readU8();
    if (value > 1) {
        _error = ReadError::InvalidBool;
        return false;
    }
    return value == 1;
}

std::span<const std::byte> Reader::readBytes(std::size_t size)
{
    return take(size);
}

std::string Reader::readString(std::size_t minLength, std::size_t maxLength)
{
    const std::size_t length = readU8();
    return readText(length, minLength, std::min<std::size_t>(maxLength, 255));
}

std::string Reader::readString16(std::size_t minLength, std::size_t maxLength)
{
    const std::size_t length = readU16();
    return readText(length, minLength, std::min<std::size_t>(maxLength, 65535));
}

std::string Reader::readText(std::size_t length, std::size_t minLength, std::size_t maxLength)
{
    if (!ok()) {
        return {};
    }
    if (length < minLength || length > maxLength) {
        _error = ReadError::InvalidLength;
        return {};
    }
    // Checked before building the string: within the range of the field, and only if the bytes are there
    const std::span<const std::byte> bytes = take(length);
    if (!ok()) {
        return {};
    }
    std::string text(length, '\0');
    std::transform(bytes.begin(), bytes.end(), text.begin(),
                   [](std::byte b) { return static_cast<char>(b); });
    if (!isValidText(text)) {
        _error = ReadError::InvalidText;
        return {};
    }
    return text;
}

void Reader::fail()
{
    if (ok()) {
        _error = ReadError::InvalidValue;
    }
}

bool Reader::finish()
{
    if (ok() && remaining() != 0) {
        _error = ReadError::TrailingBytes;
    }
    return ok();
}

} // namespace rtype::network
