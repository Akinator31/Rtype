#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

/// The cryptography the programs need outside the protocols: random bytes and SHA-256, from TF-PSA-Crypto,
/// the cryptography of Mbed TLS (ADR 0014). Its types stay inside libs/network.
namespace rtype::network
{

/// Fills the bytes from the random generator of the cryptography: for keys, tokens, salts and codes. Never
/// std::rand() nor std::random_device, which promise nothing an attacker cannot guess.
/// @throws std::runtime_error if the generator cannot be used.
void randomBytes(std::span<std::byte> bytes);

/// `Size` random bytes.
/// @throws std::runtime_error if the generator cannot be used.
template <std::size_t Size>
[[nodiscard]] std::array<std::byte, Size> randomBytes()
{
    std::array<std::byte, Size> bytes{};
    randomBytes(std::span<std::byte>(bytes));
    return bytes;
}

inline constexpr std::size_t Sha256Size = 32;
using Sha256 = std::array<std::byte, Sha256Size>;

/// The SHA-256 of the bytes.
/// @throws std::runtime_error if the cryptography cannot be used.
[[nodiscard]] Sha256 sha256(std::span<const std::byte> bytes);
/// The SHA-256 of the bytes of the text, as they are: no encoding is changed.
[[nodiscard]] Sha256 sha256(std::string_view text);

} // namespace rtype::network
