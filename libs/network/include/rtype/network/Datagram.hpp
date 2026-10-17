#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

/// The encrypted datagrams of the game (docs/protocol/game.md, sections 2 to 4): a header in clear, the body
/// encrypted with ChaCha20-Poly1305, and a tag that authenticates both.
///
/// Everything here works on bytes only, with no socket: it can be tested and fuzzed alone. A receiver uses it
/// in this order, and drops the datagram silently at the first failure:
/// @code
/// const auto header = rtype::network::parseDatagramHeader(datagram);   // size, magic, version...
/// if (!header) return;
/// Session *session = sessions.find(header->session);                    // known session?
/// if (!session || !session->window.accepts(header->packetNumber)) return; // duplicate, replay, too old?
/// const auto body = rtype::network::openDatagram(session->peerKey, datagram); // forged or changed?
/// if (!body) return;
/// session->window.mark(header->packetNumber);                           // only once authenticated
/// handle(*body);
/// @endcode
/// and a sender:
/// @code
/// const std::optional<std::uint64_t> number = session.packets.next();
/// if (!number) { /* every packet number was used: close the session */ }
/// socket.send(peer, rtype::network::sealDatagram(session.ownKey, {session.id, *number}, body));
/// @endcode
namespace rtype::network
{

/// Size of the header, sent in clear and authenticated.
inline constexpr std::size_t DatagramHeaderSize = 16;
/// Size of the authentication tag, after the encrypted body.
inline constexpr std::size_t DatagramTagSize = 16;
/// Smallest datagram: a header, an empty body and a tag.
inline constexpr std::size_t MinSealedDatagramSize = DatagramHeaderSize + DatagramTagSize;
/// Largest datagram, all fields included, so that it is never fragmented on the way.
inline constexpr std::size_t MaxSealedDatagramSize = 1200;
/// Largest body of a datagram.
inline constexpr std::size_t MaxDatagramBodySize = MaxSealedDatagramSize - MinSealedDatagramSize;
/// First field of every datagram, "RT".
inline constexpr std::uint16_t DatagramMagic = 0x5254;
/// Version of the datagram layout.
inline constexpr std::uint8_t DatagramVersion = 1;

struct DatagramHeader;

/// A 32-byte ChaCha20-Poly1305 key, with which one side encrypts the datagrams it sends. Each player has two:
/// the client key and the server key, made by the master and sent over TLS (`GAME_JOINED`).
///
/// The key is secret: it is never logged, and its bytes are erased from memory once the last copy is
/// destroyed. Copying a key is cheap: the copies share it.
class DatagramKey
{
public:
    static constexpr std::size_t Size = 32;
    using Bytes = std::array<std::byte, Size>;

    struct Impl;

private:
    std::shared_ptr<const Impl> _impl;

    friend std::vector<std::byte> sealDatagram(const DatagramKey &key, const DatagramHeader &header,
                                               std::span<const std::byte> body);
    friend std::optional<std::vector<std::byte>> openDatagram(const DatagramKey &key,
                                                              std::span<const std::byte> datagram);

public:
    /// The key with these bytes, received from the master.
    explicit DatagramKey(const Bytes &bytes);

    /// Draws a new key from the random generator of the system.
    [[nodiscard]] static DatagramKey generate();

    /// The bytes of the key, to send it to the client over TLS. Secret: never log them.
    [[nodiscard]] Bytes bytes() const;
};

/// The fields of the header that change from one datagram to the next.
struct DatagramHeader {
    /// The session given by the master, never 0.
    std::uint32_t session = 0;
    /// Number of the datagram in its direction, from 1, never used twice with the same key.
    std::uint64_t packetNumber = 0;

    friend bool operator==(const DatagramHeader &, const DatagramHeader &) = default;
};

/// Reads the header of a received datagram, before anything else: nothing if the datagram is too small or too
/// large, if its magic, version or reserved byte is wrong, or if its session or packet number is 0. The
/// header is not authenticated yet: openDatagram() does it.
[[nodiscard]] std::optional<DatagramHeader> parseDatagramHeader(std::span<const std::byte> datagram);

/// Builds a datagram: the header, then the body encrypted with the key, then the tag.
/// @throws std::length_error if the body is larger than MaxDatagramBodySize.
/// @throws std::invalid_argument if the session or the packet number is 0.
[[nodiscard]] std::vector<std::byte> sealDatagram(const DatagramKey &key, const DatagramHeader &header,
                                                  std::span<const std::byte> body);

/// Checks that a datagram was made with the key and was not changed on the way, and decrypts its body.
/// Nothing if the header is invalid (as parseDatagramHeader()) or if the tag does not match: the datagram
/// was made with another key, or a byte of it changed. Replays are not detected here: see ReplayWindow.
[[nodiscard]] std::optional<std::vector<std::byte>> openDatagram(const DatagramKey &key,
                                                                 std::span<const std::byte> datagram);

/// The packet numbers already received in a session, to drop the datagrams received twice: duplicated by the
/// network, or replayed by an attacker who recorded them (docs/protocol/game.md, section 4).
///
/// It remembers the highest number accepted and the 256 before it. A number is accepted if it is higher, or
/// within these 256 and not received yet. Older numbers are refused: a datagram this late is useless anyway.
class ReplayWindow
{
public:
    /// How many numbers before the highest one are remembered.
    static constexpr std::size_t Size = 256;

private:
    std::uint64_t _highest = 0;
    /// Bit i set: number `_highest - 1 - i` was accepted.
    std::bitset<Size> _seen;

public:
    /// Tells whether a datagram with this number may be new. Check it before decrypting, then call mark()
    /// once decrypted: a forged datagram must not move the window.
    [[nodiscard]] bool accepts(std::uint64_t packetNumber) const;

    /// Remembers that a datagram with this number was accepted. Does nothing if accepts() refuses it.
    void mark(std::uint64_t packetNumber);

    /// The highest number accepted, 0 if none: the `Ack` field of the datagrams sent back.
    [[nodiscard]] std::uint64_t highest() const { return _highest; }

    /// Which of the 32 numbers before the highest one were accepted: bit i set for `highest() - 1 - i`. The
    /// `Ack bits` field of the datagrams sent back.
    [[nodiscard]] std::uint32_t ackBits() const;
};

/// The packet numbers of the datagrams one side sends: 1, 2, 3... so that a number is never used twice with
/// the same key, which would break the encryption.
class PacketCounter
{
private:
    std::uint64_t _last = 0;

public:
    /// Starts after this number: the first datagram gets `last + 1`.
    explicit PacketCounter(std::uint64_t last = 0) : _last(last) {}

    /// The number of the next datagram, or nothing once every number was used: the session must then be
    /// closed (after 2^64 - 1 datagrams, which never happens in practice).
    [[nodiscard]] std::optional<std::uint64_t> next();

    /// The number given last, 0 if none.
    [[nodiscard]] std::uint64_t last() const { return _last; }
};

} // namespace rtype::network
