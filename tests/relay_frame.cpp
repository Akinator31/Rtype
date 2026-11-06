// Tests of the relay frame, the datagram of a game with the address of its client, between the relay of the
// master and an instance (docs/protocol/instances.md, section 5).

#include "Common/RelayFrame.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

using namespace rtype::server;
using rtype::network::Address;
using rtype::network::MaxSealedDatagramSize;
using Bytes = std::vector<std::byte>;

namespace
{

Bytes datagram(std::size_t size)
{
    Bytes bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::byte>(i * 31 + 7);
    }
    return bytes;
}

bool same(std::span<const std::byte> left, std::span<const std::byte> right)
{
    return std::ranges::equal(left, right);
}

} // namespace

TEST_CASE("A relay frame survives a round trip, IPv4 and IPv6", "[relay]")
{
    const Bytes payload = datagram(64);
    for (const Address &client :
         {*Address::parse("203.0.113.7", 51234), *Address::parse("2001:db8::42", 4242),
          *Address::parse("::1", 1), Address::loopbackIpv4(65535)}) {
        const Bytes frame = encodeRelayFrame(client, payload);
        CHECK(frame.size() == RelayFrameHeaderSize + payload.size());
        const auto decoded = decodeRelayFrame(frame);
        REQUIRE(decoded);
        CHECK(decoded->client == client);
        CHECK(same(decoded->datagram, payload));
    }
}

TEST_CASE("A relay frame is written field by field", "[relay]")
{
    const Bytes frame = encodeRelayFrame(*Address::parse("192.168.1.10", 4242), datagram(2));
    Bytes expected = {std::byte{4}, std::byte{192}, std::byte{168}, std::byte{1}, std::byte{10}};
    expected.resize(17, std::byte{0}); // the tail of an IPv4 address
    expected.push_back(std::byte{0x10});
    expected.push_back(std::byte{0x92}); // 4242, big endian
    expected.push_back(std::byte{7});
    expected.push_back(std::byte{38});
    CHECK(frame == expected);

    const Bytes v6 = encodeRelayFrame(*Address::parse("2001:db8::1", 1), datagram(1));
    CHECK(v6[0] == std::byte{6});
    CHECK(v6[1] == std::byte{0x20});
    CHECK(v6[2] == std::byte{0x01});
    CHECK(v6[16] == std::byte{0x01});
}

TEST_CASE("A relay frame carries the largest datagram of the game, not one byte more", "[relay]")
{
    const Address client = Address::loopbackIpv4(5000);
    const Bytes largest = datagram(MaxSealedDatagramSize);
    const Bytes frame = encodeRelayFrame(client, largest);
    CHECK(frame.size() == MaxRelayFrameSize);
    const auto decoded = decodeRelayFrame(frame);
    REQUIRE(decoded);
    CHECK(same(decoded->datagram, largest));

    CHECK_THROWS_AS(encodeRelayFrame(client, datagram(MaxSealedDatagramSize + 1)), std::invalid_argument);
    Bytes longer = frame;
    longer.push_back(std::byte{0});
    CHECK_FALSE(decodeRelayFrame(longer));
}

TEST_CASE("A relay frame refuses what the protocol forbids", "[relay]")
{
    const Address client = *Address::parse("198.51.100.3", 6000);
    CHECK_THROWS_AS(encodeRelayFrame(client, {}), std::invalid_argument);
    CHECK_THROWS_AS(encodeRelayFrame(client.withPort(0), datagram(10)), std::invalid_argument);

    const Bytes valid = encodeRelayFrame(client, datagram(10));
    REQUIRE(decodeRelayFrame(valid));

    // Cut anywhere, down to a header alone: no datagram
    for (std::size_t size = 0; size <= RelayFrameHeaderSize; ++size) {
        CHECK_FALSE(decodeRelayFrame(std::span(valid).first(size)));
    }
    // A family other than 4 or 6
    for (const int family : {0, 1, 5, 7, 0x46, 0xFF}) {
        Bytes frame = valid;
        frame[0] = static_cast<std::byte>(family);
        CHECK_FALSE(decodeRelayFrame(frame));
    }
    // A byte after an IPv4 address
    for (std::size_t offset = 5; offset < 17; ++offset) {
        Bytes frame = valid;
        frame[offset] = std::byte{1};
        CHECK_FALSE(decodeRelayFrame(frame));
    }
    // Port 0
    Bytes noPort = valid;
    noPort[17] = noPort[18] = std::byte{0};
    CHECK_FALSE(decodeRelayFrame(noPort));
}

TEST_CASE("Random bytes never crash the decoder of the relay frames", "[relay]")
{
    std::mt19937 random(17);
    std::uniform_int_distribution<std::size_t> size(0, MaxRelayFrameSize + 20);
    std::uniform_int_distribution<int> byte(0, 255);
    std::uniform_int_distribution<int> family(0, 2);
    int decoded = 0;
    for (int i = 0; i < 20000; ++i) {
        Bytes frame(size(random));
        for (std::byte &value : frame) {
            value = static_cast<std::byte>(byte(random));
        }
        // Often a family that decodes, so that the rest is read too
        if (!frame.empty() && family(random) != 0) {
            frame[0] = static_cast<std::byte>(family(random) == 1 ? 4 : 6);
        }
        if (const auto result = decodeRelayFrame(frame)) {
            ++decoded;
            CHECK(result->datagram.size() == frame.size() - RelayFrameHeaderSize);
            CHECK(result->client.port() != 0);
        }
    }
    CHECK(decoded > 0);
}
