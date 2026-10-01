// The binary network protocol is exchanged between clients and servers built with different compilers
// (GCC, Clang, MSVC). These tests check the platform properties it relies on.

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>

TEST_CASE("Bytes are 8 bits wide", "[platform]")
{
    STATIC_REQUIRE(CHAR_BIT == 8);
}

TEST_CASE("Fixed-width integers have their nominal size", "[platform]")
{
    STATIC_REQUIRE(sizeof(std::uint8_t) == 1);
    STATIC_REQUIRE(sizeof(std::uint16_t) == 2);
    STATIC_REQUIRE(sizeof(std::uint32_t) == 4);
    STATIC_REQUIRE(sizeof(std::uint64_t) == 8);
}

TEST_CASE("Floating-point numbers follow IEEE 754", "[platform]")
{
    STATIC_REQUIRE(std::numeric_limits<float>::is_iec559);
    STATIC_REQUIRE(sizeof(float) == 4);

    // 1.0f is 0x3F800000 in IEEE 754 single precision
    const float one = 1.0f;
    std::uint32_t bits = 0;
    std::memcpy(&bits, &one, sizeof(bits));
    REQUIRE(bits == 0x3F800000u);
}

TEST_CASE("Host byte order is known", "[platform]")
{
    // The protocol uses network byte order (big-endian); conversions must know the host order
    STATIC_REQUIRE((std::endian::native == std::endian::little || std::endian::native == std::endian::big));
}
