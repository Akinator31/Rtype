// Tests of the cryptography of libs/network outside the protocols: random bytes and SHA-256
// (rtype/network/Crypto.hpp).

#include "rtype/network/Crypto.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <set>
#include <string>

using namespace rtype::network;

namespace
{

std::string hex(const Sha256 &hash)
{
    static constexpr std::string_view Digits = "0123456789abcdef";
    std::string text;
    for (const std::byte byte : hash) {
        text.push_back(Digits[std::to_integer<unsigned>(byte) >> 4]);
        text.push_back(Digits[std::to_integer<unsigned>(byte) & 0x0F]);
    }
    return text;
}

} // namespace

TEST_CASE("SHA-256 gives the digests of FIPS 180-2", "[network][crypto]")
{
    CHECK(hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::array<std::byte, 3> bytes{std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
    CHECK(sha256(std::span<const std::byte>(bytes)) == sha256("abc"));
}

TEST_CASE("Random bytes differ from one draw to the next", "[network][crypto]")
{
    std::set<std::array<std::byte, 16>> draws;
    for (int i = 0; i < 100; ++i) {
        draws.insert(randomBytes<16>());
    }
    CHECK(draws.size() == 100);

    // Every byte of the buffer is drawn
    std::array<std::byte, 64> buffer{};
    randomBytes(buffer);
    CHECK(buffer != std::array<std::byte, 64>{});
}
