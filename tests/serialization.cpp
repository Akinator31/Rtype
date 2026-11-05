// Tests of the binary serialization of libs/network: Writer, Reader and text validation, against the
// conventions of the protocol specification (docs/protocol/index.md, section 2).

#include "rtype/network/Reader.hpp"
#include "rtype/network/Text.hpp"
#include "rtype/network/Writer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rtype::network;

namespace
{

std::vector<std::byte> bytes(std::initializer_list<int> values)
{
    std::vector<std::byte> result;
    for (const int value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

std::vector<std::byte> written(const Writer &writer)
{
    return {writer.bytes().begin(), writer.bytes().end()};
}

} // namespace

// --- Writer ---

TEST_CASE("Integers are written big-endian", "[network]")
{
    Writer writer;
    writer.writeU8(0x12);
    writer.writeU16(0x1234);
    writer.writeU32(0x12345678);
    writer.writeU64(0x0102030405060708);

    CHECK(written(writer) == bytes({0x12, 0x12, 0x34, 0x12, 0x34, 0x56, 0x78, 1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST_CASE("Signed integers are written in two's complement", "[network]")
{
    Writer writer;
    writer.writeI16(-1);
    writer.writeI32(-2);

    CHECK(written(writer) == bytes({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE}));
}

TEST_CASE("Booleans are written as 0 and 1", "[network]")
{
    Writer writer;
    writer.writeBool(false);
    writer.writeBool(true);

    CHECK(written(writer) == bytes({0, 1}));
}

TEST_CASE("A str8 is written as its length then its bytes", "[network]")
{
    Writer writer;
    writer.writeString("ab");
    writer.writeString("");

    CHECK(written(writer) == bytes({2, 'a', 'b', 0}));
}

TEST_CASE("A str8 longer than 255 bytes is refused", "[network]")
{
    Writer writer;

    CHECK_NOTHROW(writer.writeString(std::string(255, 'a')));
    CHECK_THROWS_AS(writer.writeString(std::string(256, 'a')), std::length_error);
}

TEST_CASE("A str8 with a control character or invalid UTF-8 is refused", "[network]")
{
    Writer writer;

    CHECK_THROWS_AS(writer.writeString("line\nbreak"), std::invalid_argument);
    CHECK_THROWS_AS(writer.writeString("\xC3"), std::invalid_argument);
}

TEST_CASE("A str16 is written as its length on two bytes then its bytes", "[network]")
{
    Writer writer;
    writer.writeString16("ab");
    writer.writeString16("");

    CHECK(written(writer) == bytes({0, 2, 'a', 'b', 0, 0}));
}

TEST_CASE("A str16 holds more than 255 bytes, and refuses what a str8 refuses", "[network]")
{
    Writer writer;

    CHECK_NOTHROW(writer.writeString16(std::string(300, 'a')));
    CHECK(writer.size() == 302);
    CHECK_NOTHROW(writer.writeString16(std::string(65535, 'a')));
    CHECK_THROWS_AS(writer.writeString16(std::string(65536, 'a')), std::length_error);
    CHECK_THROWS_AS(writer.writeString16("line\nbreak"), std::invalid_argument);
    CHECK_THROWS_AS(writer.writeString16("\xC3"), std::invalid_argument);
}

TEST_CASE("patchU16 replaces two bytes already written", "[network]")
{
    Writer writer;
    writer.writeU16(0);
    writer.writeU8(7);
    writer.patchU16(0, 0xABCD);

    CHECK(written(writer) == bytes({0xAB, 0xCD, 7}));
    CHECK_THROWS_AS(writer.patchU16(2, 1), std::out_of_range);
    CHECK_THROWS_AS(writer.patchU16(10, 1), std::out_of_range);
}

TEST_CASE("take() returns the bytes and empties the writer", "[network]")
{
    Writer writer;
    writer.writeU8(1);

    CHECK(writer.take() == bytes({1}));
    CHECK(writer.size() == 0);
}

// --- Reader ---

TEST_CASE("Every type survives a round trip", "[network]")
{
    const std::array<std::byte, 4> key{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    Writer writer;
    writer.writeU8(std::numeric_limits<std::uint8_t>::max());
    writer.writeU16(std::numeric_limits<std::uint16_t>::max());
    writer.writeU32(std::numeric_limits<std::uint32_t>::max());
    writer.writeU64(std::numeric_limits<std::uint64_t>::max());
    writer.writeI16(std::numeric_limits<std::int16_t>::min());
    writer.writeI32(std::numeric_limits<std::int32_t>::min());
    writer.writeI16(std::numeric_limits<std::int16_t>::max());
    writer.writeI32(-1);
    writer.writeBool(true);
    writer.writeBytes(key);
    writer.writeString("Pavel");

    Reader reader(writer.bytes());
    CHECK(reader.readU8() == std::numeric_limits<std::uint8_t>::max());
    CHECK(reader.readU16() == std::numeric_limits<std::uint16_t>::max());
    CHECK(reader.readU32() == std::numeric_limits<std::uint32_t>::max());
    CHECK(reader.readU64() == std::numeric_limits<std::uint64_t>::max());
    CHECK(reader.readI16() == std::numeric_limits<std::int16_t>::min());
    CHECK(reader.readI32() == std::numeric_limits<std::int32_t>::min());
    CHECK(reader.readI16() == std::numeric_limits<std::int16_t>::max());
    CHECK(reader.readI32() == -1);
    CHECK(reader.readBool());
    CHECK(reader.readBytes<4>() == key);
    CHECK(reader.readString() == "Pavel");
    CHECK(reader.finish());
}

TEST_CASE("The HELLO frame of the specification is written and read back byte for byte", "[network]")
{
    // docs/protocol/lobby.md, section 3: HELLO with version 1 and the name "Pavel"
    const std::vector<std::byte> frame =
        bytes({0x00, 0x09, 0x01, 0x00, 0x01, 0x05, 0x50, 0x61, 0x76, 0x65, 0x6C});

    Writer writer;
    writer.writeU16(0); // size, known at the end
    writer.writeU8(0x01);
    writer.writeU16(1);
    writer.writeString("Pavel");
    writer.patchU16(0, static_cast<std::uint16_t>(writer.size() - 2));
    CHECK(written(writer) == frame);

    Reader reader(frame);
    CHECK(reader.readU16() == 9);
    CHECK(reader.readU8() == 0x01);
    CHECK(reader.readU16() == 1);
    CHECK(reader.readString(1, 16) == "Pavel");
    CHECK(reader.finish());
}

TEST_CASE("Reading past the end fails without reading anything", "[network]")
{
    const std::vector<std::byte> data = bytes({0x12});
    Reader reader(data);

    CHECK(reader.readU16() == 0);
    CHECK_FALSE(reader.ok());
    CHECK(reader.error() == ReadError::TooShort);
    CHECK(reader.remaining() == 1);
}

TEST_CASE("Once failed, a reader keeps failing and keeps its first error", "[network]")
{
    const std::vector<std::byte> data = bytes({2, 0x12, 0x34});
    Reader reader(data);

    CHECK_FALSE(reader.readBool());
    CHECK(reader.error() == ReadError::InvalidBool);
    CHECK(reader.readU16() == 0); // the bytes are there, but the reader has failed
    reader.fail();
    CHECK(reader.error() == ReadError::InvalidBool);
    CHECK_FALSE(reader.finish());
}

TEST_CASE("A bool other than 0 or 1 is refused", "[network]")
{
    const std::vector<std::byte> data = bytes({0, 1, 2});
    Reader reader(data);

    CHECK_FALSE(reader.readBool());
    CHECK(reader.readBool());
    CHECK_FALSE(reader.readBool());
    CHECK(reader.error() == ReadError::InvalidBool);
}

TEST_CASE("A str8 whose bytes are missing is refused", "[network]")
{
    const std::vector<std::byte> data = bytes({5, 'a', 'b'});
    Reader reader(data);

    CHECK(reader.readString().empty());
    CHECK(reader.error() == ReadError::TooShort);
}

TEST_CASE("A str8 outside the length range of its field is refused", "[network]")
{
    const std::vector<std::byte> empty = bytes({0});
    const std::vector<std::byte> tooLong = bytes({3, 'a', 'b', 'c'});

    Reader emptyReader(empty);
    CHECK(emptyReader.readString(1, 16).empty());
    CHECK(emptyReader.error() == ReadError::InvalidLength);

    Reader longReader(tooLong);
    CHECK(longReader.readString(1, 2).empty());
    CHECK(longReader.error() == ReadError::InvalidLength);
}

TEST_CASE("A str8 with a control character or invalid UTF-8 is not read", "[network]")
{
    const std::vector<std::byte> control = bytes({2, 'a', 0x00});
    const std::vector<std::byte> invalid = bytes({1, 0xFF});

    Reader controlReader(control);
    CHECK(controlReader.readString().empty());
    CHECK(controlReader.error() == ReadError::InvalidText);

    Reader invalidReader(invalid);
    CHECK(invalidReader.readString().empty());
    CHECK(invalidReader.error() == ReadError::InvalidText);
}

TEST_CASE("A str16 is read back, within the length range of its field", "[network]")
{
    Writer writer;
    writer.writeString16(std::string(300, 'p'));
    const std::vector<std::byte> long300 = writer.take();

    Reader reader(long300);
    CHECK(reader.readString16(1, 512) == std::string(300, 'p'));
    CHECK(reader.finish());

    Reader tooLong(long300);
    CHECK(tooLong.readString16(1, 299).empty());
    CHECK(tooLong.error() == ReadError::InvalidLength);

    const std::vector<std::byte> cut = bytes({0, 3, 'a', 'b'});
    Reader cutReader(cut);
    CHECK(cutReader.readString16().empty());
    CHECK(cutReader.error() == ReadError::TooShort);

    const std::vector<std::byte> control = bytes({0, 2, 'a', 0x07});
    Reader controlReader(control);
    CHECK(controlReader.readString16().empty());
    CHECK(controlReader.error() == ReadError::InvalidText);
}

TEST_CASE("Bytes left after the last field make finish() fail", "[network]")
{
    const std::vector<std::byte> data = bytes({1, 2});
    Reader reader(data);

    CHECK(reader.readU8() == 1);
    CHECK_FALSE(reader.finish());
    CHECK(reader.error() == ReadError::TrailingBytes);
}

TEST_CASE("fail() lets a decoder refuse a value", "[network]")
{
    const std::vector<std::byte> data = bytes({9});
    Reader reader(data);

    const std::uint8_t maxPlayers = reader.readU8();
    if (maxPlayers < 1 || maxPlayers > 4) {
        reader.fail();
    }

    CHECK(reader.error() == ReadError::InvalidValue);
    CHECK_FALSE(reader.finish());
}

TEST_CASE("An announced count larger than the data stops at the end of the data", "[network]")
{
    // A GAME_LIST that announces 65535 games but carries one: a decoder that loops while the reader is ok
    // stops after the data, without reserving room for 65535 entries
    Writer writer;
    writer.writeU16(65535);
    writer.writeU32(1);
    writer.writeString("Game");
    writer.writeU8(1);
    writer.writeU8(4);
    Reader reader(writer.bytes());

    const std::uint16_t count = reader.readU16();
    std::size_t decoded = 0;
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        (void)reader.readU32();
        (void)reader.readString(1, 32);
        (void)reader.readU8();
        (void)reader.readU8();
        if (reader.ok()) {
            ++decoded;
        }
    }

    CHECK(decoded == 1);
    CHECK(reader.error() == ReadError::TooShort);
}

TEST_CASE("Read errors have a name for the logs", "[network]")
{
    CHECK(toString(ReadError::None) == "no error");
    CHECK(toString(ReadError::TooShort) == "too short");
    CHECK(toString(ReadError::TrailingBytes) == "trailing bytes");
}

// --- Text ---

TEST_CASE("Text in any language is valid", "[network]")
{
    CHECK(isValidText(""));
    CHECK(isValidText("Pavel"));
    CHECK(isValidText("Zoé"));              // 2-byte characters
    CHECK(isValidText("東京"));             // 3-byte characters
    CHECK(isValidText("\xF0\x9F\x9A\x80")); // U+1F680, a 4-byte character
    CHECK(isValidText("\xF4\x8F\xBF\xBF")); // U+10FFFF, the last character
}

TEST_CASE("ASCII control characters are not valid text", "[network]")
{
    CHECK_FALSE(isValidText(std::string("a\0b", 3)));
    CHECK_FALSE(isValidText("tab\there"));
    CHECK_FALSE(isValidText("\x1B[31mred"));
    CHECK_FALSE(isValidText("\x7F"));
}

TEST_CASE("Malformed UTF-8 is not valid text", "[network]")
{
    CHECK_FALSE(isValidText("\x80"));             // continuation byte alone
    CHECK_FALSE(isValidText("\xC3"));             // cut character
    CHECK_FALSE(isValidText("\xC3\x28"));         // bad continuation byte
    CHECK_FALSE(isValidText("\xC0\xAF"));         // overlong "/"
    CHECK_FALSE(isValidText("\xE0\x80\xAF"));     // overlong "/"
    CHECK_FALSE(isValidText("\xED\xA0\x80"));     // U+D800, a UTF-16 surrogate
    CHECK_FALSE(isValidText("\xF4\x90\x80\x80")); // U+110000, above the last character
    CHECK_FALSE(isValidText("\xF8\x88\x80\x80\x80"));
    CHECK_FALSE(isValidText("\xFF"));
}
