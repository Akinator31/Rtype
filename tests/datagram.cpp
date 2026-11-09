// Tests of the encrypted datagrams of libs/network: keys, headers, encryption, replay window and packet
// numbers (docs/protocol/game.md, sections 2 to 4).

#include "rtype/network/Datagram.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace rtype::network;

namespace
{

std::vector<std::byte> fromHex(std::string_view hex)
{
    std::vector<std::byte> bytes;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::byte>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16)));
    }
    return bytes;
}

std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::vector<std::byte> payloadOf(std::size_t size)
{
    std::vector<std::byte> payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<std::byte>(i * 7);
    }
    return payload;
}

// The key of the example of RFC 8439, section 2.8.2: bytes 0x80 to 0x9f
DatagramKey testKey()
{
    DatagramKey::Bytes bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::byte>(0x80 + i);
    }
    return DatagramKey(bytes);
}

} // namespace

// --- Encryption ---

// Computed with another implementation of RFC 8439 (Python's cryptography package), from the layout of
// docs/protocol/game.md: what an independent client must produce
TEST_CASE("A datagram is the one the specification describes", "[network]")
{
    const DatagramKey key = testKey();

    CHECK(sealDatagram(key, {42, 1}, bytesOf("R-Type")) ==
          fromHex("525405000000002a00000000000000012c4395abd9e2c26b4bab67f09ad0fe84c4fd9b4d584f"));
    CHECK(sealDatagram(key, {7, 0x0102030405060708}, {}) ==
          fromHex("5254050000000007010203040506070812a8aa81b8f4778ee6a4b013f669bf1a"));
}

TEST_CASE("A datagram opens with its key and gives back its body", "[network]")
{
    const DatagramKey key = DatagramKey::generate();

    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{100}, MaxDatagramBodySize}) {
        const std::vector<std::byte> body = payloadOf(size);
        const std::vector<std::byte> datagram = sealDatagram(key, {3, 9}, body);

        CHECK(datagram.size() == MinSealedDatagramSize + size);
        CHECK(parseDatagramHeader(datagram) == DatagramHeader{3, 9});
        CHECK(openDatagram(key, datagram) == body);
    }
}

TEST_CASE("The body is not readable in the datagram", "[network]")
{
    const std::vector<std::byte> body = bytesOf("secret position of the player");
    const std::vector<std::byte> datagram = sealDatagram(DatagramKey::generate(), {1, 1}, body);

    const std::span<const std::byte> sealed = std::span(datagram).subspan(DatagramHeaderSize, body.size());
    CHECK(std::vector(sealed.begin(), sealed.end()) != body);
}

TEST_CASE("A datagram cannot be sealed with a body too large, or a session or packet number of 0",
          "[network]")
{
    const DatagramKey key = DatagramKey::generate();

    CHECK_THROWS_AS(sealDatagram(key, {1, 1}, payloadOf(MaxDatagramBodySize + 1)), std::length_error);
    CHECK_THROWS_AS(sealDatagram(key, {0, 1}, {}), std::invalid_argument);
    CHECK_THROWS_AS(sealDatagram(key, {1, 0}, {}), std::invalid_argument);
}

TEST_CASE("A datagram changed anywhere, even in its header, does not open", "[network]")
{
    const DatagramKey key = DatagramKey::generate();
    const std::vector<std::byte> datagram = sealDatagram(key, {5, 12}, bytesOf("move up"));

    for (std::size_t i = 0; i < datagram.size(); ++i) {
        for (const auto bit : {std::byte{0x01}, std::byte{0x80}}) {
            std::vector<std::byte> changed = datagram;
            changed[i] ^= bit;
            CAPTURE(i, std::to_integer<int>(bit));
            CHECK_FALSE(openDatagram(key, changed));
        }
    }
    const std::vector<std::byte> truncated(datagram.begin(), datagram.end() - 1);
    std::vector<std::byte> extended = datagram;
    extended.push_back(std::byte{0});
    CHECK_FALSE(openDatagram(key, truncated));
    CHECK_FALSE(openDatagram(key, extended));
}

TEST_CASE("A datagram does not open with another key", "[network]")
{
    const std::vector<std::byte> datagram = sealDatagram(DatagramKey::generate(), {1, 1}, bytesOf("fire"));

    CHECK_FALSE(openDatagram(DatagramKey::generate(), datagram));
}

TEST_CASE("Random bytes never open, and never crash", "[network]")
{
    const DatagramKey key = DatagramKey::generate();
    std::mt19937 random(1234);
    std::uniform_int_distribution<std::size_t> size(0, MaxSealedDatagramSize + 10);
    std::uniform_int_distribution<int> byte(0, 255);

    for (int i = 0; i < 2000; ++i) {
        std::vector<std::byte> datagram(size(random));
        for (auto &value : datagram) {
            value = static_cast<std::byte>(byte(random));
        }
        // A valid header half of the time, to reach the decryption
        if (datagram.size() >= DatagramHeaderSize && i % 2 == 0) {
            const std::vector<std::byte> header = sealDatagram(key, {1, 1}, {});
            std::copy_n(header.begin(), DatagramHeaderSize, datagram.begin());
        }
        CHECK_FALSE(openDatagram(key, datagram));
    }
}

// --- Header ---

TEST_CASE("A header is refused if the datagram is too small or too large", "[network]")
{
    const DatagramKey key = DatagramKey::generate();
    std::vector<std::byte> datagram = sealDatagram(key, {1, 1}, payloadOf(MaxDatagramBodySize));

    CHECK(parseDatagramHeader(datagram));
    datagram.push_back(std::byte{0});
    CHECK_FALSE(parseDatagramHeader(datagram));
    datagram.resize(MinSealedDatagramSize);
    CHECK(parseDatagramHeader(datagram));
    datagram.pop_back();
    CHECK_FALSE(parseDatagramHeader(datagram));
    CHECK_FALSE(parseDatagramHeader({}));
}

TEST_CASE("A header is refused if a fixed field is wrong, or if its session or packet number is 0",
          "[network]")
{
    const std::vector<std::byte> datagram = sealDatagram(DatagramKey::generate(), {1, 1}, {});
    REQUIRE(parseDatagramHeader(datagram));

    // Magic, magic, version, reserved, last byte of the session, last byte of the packet number
    for (const std::size_t offset : {0, 1, 2, 3, 7, 15}) {
        std::vector<std::byte> changed = datagram;
        changed[offset] = offset == 7 || offset == 15 ? std::byte{0} : std::byte{0x7F};
        CAPTURE(offset);
        CHECK_FALSE(parseDatagramHeader(changed));
    }
}

TEST_CASE("A header reads the largest session and packet number", "[network]")
{
    const DatagramHeader largest{std::numeric_limits<std::uint32_t>::max(),
                                 std::numeric_limits<std::uint64_t>::max()};
    const DatagramKey key = DatagramKey::generate();

    const std::vector<std::byte> datagram = sealDatagram(key, largest, bytesOf("x"));

    CHECK(parseDatagramHeader(datagram) == largest);
    CHECK(openDatagram(key, datagram) == bytesOf("x"));
}

// --- Keys ---

TEST_CASE("A key made from the bytes of another one opens its datagrams", "[network]")
{
    const DatagramKey master = DatagramKey::generate();
    const DatagramKey client(master.bytes());
    const DatagramKey copy = client; // NOLINT(performance-unnecessary-copy-initialization): a copy is tested

    const std::vector<std::byte> datagram = sealDatagram(master, {1, 1}, bytesOf("hello"));

    CHECK(openDatagram(client, datagram) == bytesOf("hello"));
    CHECK(openDatagram(copy, datagram) == bytesOf("hello"));
}

TEST_CASE("Generated keys are all different", "[network]")
{
    CHECK(DatagramKey::generate().bytes() != DatagramKey::generate().bytes());
}

TEST_CASE("Datagrams can be sealed and opened from several threads at once", "[network]")
{
    const DatagramKey key = DatagramKey::generate();
    std::vector<std::thread> threads;
    std::vector<int> opened(4, 0);

    for (std::size_t t = 0; t < opened.size(); ++t) {
        threads.emplace_back([&, t] {
            for (std::uint64_t number = 1; number <= 200; ++number) {
                const auto body = payloadOf(number);
                if (openDatagram(key, sealDatagram(key, {static_cast<std::uint32_t>(t + 1), number}, body)) ==
                    body) {
                    ++opened[t];
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    for (const int count : opened) {
        CHECK(count == 200);
    }
}

// --- ReplayWindow ---

TEST_CASE("A replay window accepts each packet number once", "[network]")
{
    ReplayWindow window;

    CHECK_FALSE(window.accepts(0));
    CHECK(window.accepts(1));
    window.mark(1);
    CHECK_FALSE(window.accepts(1));
    CHECK(window.highest() == 1);
    window.mark(1);
    CHECK(window.highest() == 1);
}

TEST_CASE("A replay window accepts late numbers once, within its size", "[network]")
{
    ReplayWindow window;
    window.mark(300);

    CHECK(window.accepts(299));
    CHECK(window.accepts(300 - ReplayWindow::Size));
    CHECK_FALSE(window.accepts(300 - ReplayWindow::Size - 1));
    CHECK_FALSE(window.accepts(300));

    window.mark(299);
    window.mark(300 - ReplayWindow::Size);
    CHECK_FALSE(window.accepts(299));
    CHECK_FALSE(window.accepts(300 - ReplayWindow::Size));
    CHECK(window.accepts(298));
    CHECK(window.highest() == 300);
}

TEST_CASE("A replay window remembers the numbers it accepted when it moves", "[network]")
{
    ReplayWindow window;
    window.mark(10);
    window.mark(8);

    window.mark(12);
    CHECK_FALSE(window.accepts(12));
    CHECK_FALSE(window.accepts(10));
    CHECK_FALSE(window.accepts(8));
    CHECK(window.accepts(11));
    CHECK(window.accepts(9));

    // Exactly the size of the window: the old highest number is its last one
    window.mark(12 + ReplayWindow::Size);
    CHECK_FALSE(window.accepts(12));
    CHECK(window.accepts(13));
    CHECK_FALSE(window.accepts(11));

    // Further than the window: everything before is forgotten, and too old
    window.mark(10000);
    CHECK(window.accepts(9999));
    CHECK(window.accepts(10000 - ReplayWindow::Size));
    CHECK_FALSE(window.accepts(12 + ReplayWindow::Size));
}

TEST_CASE("A replay window handles numbers in any order", "[network]")
{
    ReplayWindow window;
    std::vector<std::uint64_t> numbers;
    for (std::uint64_t number = 1; number <= 1000; ++number) {
        numbers.push_back(number);
    }
    // Shuffled in small groups, as the network does: within the window
    std::mt19937 random(42);
    for (std::size_t start = 0; start < numbers.size(); start += 50) {
        std::shuffle(numbers.begin() + static_cast<std::ptrdiff_t>(start),
                     numbers.begin() + static_cast<std::ptrdiff_t>(start + 50), random);
    }

    for (const std::uint64_t number : numbers) {
        CAPTURE(number);
        REQUIRE(window.accepts(number));
        window.mark(number);
        REQUIRE_FALSE(window.accepts(number));
    }
    for (std::uint64_t number = 1000 - ReplayWindow::Size; number <= 1000; ++number) {
        CHECK_FALSE(window.accepts(number));
    }
    CHECK(window.highest() == 1000);
}

TEST_CASE("A replay window accepts the largest packet number", "[network]")
{
    ReplayWindow window;
    const std::uint64_t largest = std::numeric_limits<std::uint64_t>::max();

    window.mark(largest);

    CHECK(window.highest() == largest);
    CHECK_FALSE(window.accepts(largest));
    CHECK(window.accepts(largest - 1));
}

// --- PacketCounter ---

TEST_CASE("Packet numbers start at 1 and increase", "[network]")
{
    PacketCounter counter;

    CHECK(counter.last() == 0);
    CHECK(counter.next() == 1);
    CHECK(counter.next() == 2);
    CHECK(counter.next() == 3);
    CHECK(counter.last() == 3);
}

TEST_CASE("Packet numbers stop instead of going back to 0", "[network]")
{
    PacketCounter counter(std::numeric_limits<std::uint64_t>::max() - 1);

    CHECK(counter.next() == std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE(counter.next());
    CHECK_FALSE(counter.next());
}

TEST_CASE("A replay window gives the acknowledgement bits of the 32 numbers before the highest", "[network]")
{
    ReplayWindow window;
    CHECK(window.ackBits() == 0);

    // 37, 38, 40 and 41 received, 39 lost
    for (const std::uint64_t number : {37, 38, 40, 41}) {
        window.mark(number);
    }
    CHECK(window.highest() == 41);
    CHECK(window.ackBits() == 0b1101);

    // Only the 32 before count
    window.mark(41 + 33);
    CHECK(window.ackBits() == 0);
    window.mark(41 + 34);
    CHECK(window.ackBits() == 0b1);
}
