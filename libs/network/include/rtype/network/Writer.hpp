#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace rtype::network
{

/// Writes the fields of a message as bytes, following the conventions of the protocol: big-endian integers,
/// no padding between fields (see docs/protocol/index.md, section 2).
///
/// @code
/// rtype::network::Writer writer;
/// writer.writeU8(0x01);         // type: HELLO
/// writer.writeU16(1);           // version
/// writer.writeString("Pavel");  // name, str8
/// socket.send(address, writer.bytes());
/// @endcode
///
/// Writing never fails because of the data's size: the buffer grows. Values that the protocol forbids are
/// programming errors, reported with an exception.
class Writer
{
private:
    std::vector<std::byte> _bytes;

public:
    Writer() = default;
    /// Reserves room for this many bytes, to avoid growing the buffer while writing.
    explicit Writer(std::size_t capacity);

    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeU64(std::uint64_t value);
    void writeI16(std::int16_t value);
    void writeI32(std::int32_t value);
    /// Writes `1` for true, `0` for false.
    void writeBool(bool value);

    /// Writes raw bytes, without length: a `bytes[N]` field, whose size the reader knows.
    void writeBytes(std::span<const std::byte> bytes);
    template <std::size_t N>
    void writeBytes(const std::array<std::byte, N> &bytes)
    {
        writeBytes(std::span<const std::byte>(bytes));
    }

    /// Writes a `str8` field: its length on one byte, then its UTF-8 bytes.
    /// @throws std::length_error if the text is longer than 255 bytes.
    /// @throws std::invalid_argument if the text is not valid UTF-8, or contains a control character (see
    /// isValidText()).
    void writeString(std::string_view text);
    /// Writes a `str16` field: its length on two bytes, then its UTF-8 bytes.
    /// @throws std::length_error if the text is longer than 65535 bytes.
    /// @throws std::invalid_argument as writeString().
    void writeString16(std::string_view text);

    /// Replaces two bytes already written with a `u16`, for a field known only at the end, such as the size
    /// of a TCP frame.
    /// @throws std::out_of_range if the two bytes have not been written yet.
    void patchU16(std::size_t offset, std::uint16_t value);

    /// Number of bytes written.
    [[nodiscard]] std::size_t size() const { return _bytes.size(); }
    /// The bytes written so far.
    [[nodiscard]] std::span<const std::byte> bytes() const { return _bytes; }
    /// Takes the bytes written, leaving the writer empty.
    [[nodiscard]] std::vector<std::byte> take();
};

} // namespace rtype::network
