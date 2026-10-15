#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace rtype::network
{

/// Why a Reader failed: the first problem met.
enum class ReadError {
    /// No problem.
    None,
    /// A field goes past the end of the data.
    TooShort,
    /// A `bool` other than 0 or 1.
    InvalidBool,
    /// A `str8` whose length is out of the range of its field.
    InvalidLength,
    /// A `str8` that is not valid UTF-8, or contains a control character.
    InvalidText,
    /// Bytes are left after the last field (see Reader::finish()).
    TrailingBytes,
    /// A value refused by the decoder itself (see Reader::fail()).
    InvalidValue,
};

/// Name of a read error, for the logs: "too short", "invalid bool"...
[[nodiscard]] std::string_view toString(ReadError error);

/// Reads the fields of a message from received bytes, following the conventions of the protocol: big-endian
/// integers, no padding between fields (see docs/protocol/index.md, section 2).
///
/// The reader never reads past the end of the data. The first field that is missing or invalid puts it in an
/// **error state**: that read and every later one return 0, false or an empty value, and error() tells what
/// went wrong. So a decoder reads every field, then checks once, at the end:
/// @code
/// rtype::network::Reader reader(payload);
/// const std::uint16_t version = reader.readU16();
/// const std::string name = reader.readString(1, 16);
/// if (!reader.finish()) {
///     logger.debug("HELLO dropped: {}", toString(reader.error()));
///     return std::nullopt;
/// }
/// @endcode
///
/// Reading has no side effect besides the reader itself, and allocates no more memory than the data it reads:
/// decoders built on it can be fuzzed.
class Reader
{
private:
    std::span<const std::byte> _data;
    std::size_t _position = 0;
    ReadError _error = ReadError::None;

    /// The next `size` bytes, or an empty span (and the error state) if there are not that many left.
    std::span<const std::byte> take(std::size_t size);
    std::uint64_t readUnsigned(std::size_t size);

public:
    /// Reads from these bytes, which must stay valid while the reader is used.
    explicit Reader(std::span<const std::byte> data);

    [[nodiscard]] std::uint8_t readU8();
    [[nodiscard]] std::uint16_t readU16();
    [[nodiscard]] std::uint32_t readU32();
    [[nodiscard]] std::uint64_t readU64();
    [[nodiscard]] std::int16_t readI16();
    [[nodiscard]] std::int32_t readI32();
    /// Reads a `bool`: 0 or 1. Any other value is an error (ReadError::InvalidBool).
    [[nodiscard]] bool readBool();

    /// Reads raw bytes, a `bytes[N]` field. The span points into the reader's data: copy it to keep it.
    /// @return The bytes, or an empty span on error.
    [[nodiscard]] std::span<const std::byte> readBytes(std::size_t size);
    /// Reads a `bytes[N]` field of a size known at compile time, such as a key, as a copy.
    template <std::size_t N>
    [[nodiscard]] std::array<std::byte, N> readBytes()
    {
        std::array<std::byte, N> bytes{};
        const std::span<const std::byte> read = take(N);
        for (std::size_t i = 0; i < read.size(); ++i) {
            bytes[i] = read[i];
        }
        return bytes;
    }

    /// Reads a `str8` field: its length on one byte, then its bytes, which must be valid UTF-8 without
    /// control characters (see isValidText()).
    /// @param minLength Smallest length accepted, in bytes.
    /// @param maxLength Largest length accepted, in bytes, at most 255.
    /// @return The text, or an empty string on error.
    [[nodiscard]] std::string readString(std::size_t minLength = 0, std::size_t maxLength = 255);

    /// Puts the reader in the error state (ReadError::InvalidValue), for a value the decoder refuses, such as
    /// a number of players above 4. Does nothing if the reader has already failed.
    void fail();

    /// Checks that every byte was read: a message must end exactly after its last field. Bytes left are an
    /// error (ReadError::TrailingBytes).
    /// @return true if the reader has not failed and every byte was read.
    [[nodiscard]] bool finish();

    /// true while no read has failed.
    [[nodiscard]] bool ok() const { return _error == ReadError::None; }
    /// The first problem met, or ReadError::None.
    [[nodiscard]] ReadError error() const { return _error; }
    /// Number of bytes not read yet.
    [[nodiscard]] std::size_t remaining() const { return _data.size() - _position; }
};

} // namespace rtype::network
