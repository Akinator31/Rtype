#include "Common/Secret.hpp"

namespace rtype::server
{

bool sameSecret(const Secret &first, const Secret &second)
{
    std::byte difference{0};
    for (std::size_t i = 0; i < first.size(); ++i) {
        difference |= first[i] ^ second[i];
    }
    return difference == std::byte{0};
}

std::string formatSecret(const Secret &secret)
{
    constexpr std::string_view Digits = "0123456789abcdef";
    std::string text;
    text.reserve(SecretSize * 2);
    for (const std::byte byte : secret) {
        const auto value = std::to_integer<unsigned>(byte);
        text.push_back(Digits[value >> 4]);
        text.push_back(Digits[value & 0xF]);
    }
    return text;
}

std::optional<Secret> parseSecret(std::string_view line)
{
    if (line.ends_with('\r')) {
        line.remove_suffix(1);
    }
    if (line.size() != SecretSize * 2) {
        return std::nullopt;
    }
    // Lowercase only, as the protocol says: one way to write each secret
    const auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        return -1;
    };
    Secret secret{};
    for (std::size_t i = 0; i < SecretSize; ++i) {
        const int high = digit(line[i * 2]);
        const int low = digit(line[(i * 2) + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        secret[i] = static_cast<std::byte>((high << 4) | low);
    }
    return secret;
}

std::optional<Secret> readSecret(std::istream &input)
{
    std::string line;
    if (!std::getline(input, line)) {
        return std::nullopt;
    }
    return parseSecret(line);
}

} // namespace rtype::server
