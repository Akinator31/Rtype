#pragma once

#include <array>
#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <string_view>

namespace rtype::server
{

/// What an instance proves it was started by the master with: 32 random bytes, given on its standard input
/// (docs/protocol/instances.md, section 1). Never in the arguments, which every user of the machine can see.
inline constexpr std::size_t SecretSize = 32;
using Secret = std::array<std::byte, SecretSize>;

/// Compares two secrets in constant time: the time taken tells nothing about where they differ.
[[nodiscard]] bool sameSecret(const Secret &first, const Secret &second);

/// The secret as the master writes it: 64 lowercase hexadecimal digits, without the line feed.
[[nodiscard]] std::string formatSecret(const Secret &secret);

/// Reads a secret written by formatSecret(), without its line feed. A carriage return before it is accepted,
/// for the pipes of Windows.
/// @return Nothing if the line is anything else.
[[nodiscard]] std::optional<Secret> parseSecret(std::string_view line);

/// Reads the line of the secret from a stream, the standard input of an instance.
/// @return Nothing if the stream ends first, or if the line is not a secret.
[[nodiscard]] std::optional<Secret> readSecret(std::istream &input);

} // namespace rtype::server
