#include "Common/Base64.hpp"

#include <algorithm>
#include <cstdint>

namespace rtype::server
{

namespace
{

constexpr std::string_view Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string base64(std::string_view bytes)
{
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::size_t count = std::min<std::size_t>(3, bytes.size() - i);
        std::uint32_t group = 0;
        for (std::size_t j = 0; j < 3; ++j) {
            group <<= 8U;
            if (j < count) {
                group |= static_cast<unsigned char>(bytes[i + j]);
            }
        }
        for (std::size_t j = 0; j < 4; ++j) {
            result += j <= count ? Alphabet[(group >> (18U - 6U * j)) & 0x3FU] : '=';
        }
    }
    return result;
}

std::optional<std::string> decodeBase64(std::string_view text)
{
    if (text.size() % 4 != 0) {
        return std::nullopt;
    }
    std::string result;
    result.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        std::uint32_t group = 0;
        std::size_t padding = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const char c = text[i + j];
            group <<= 6U;
            if (c == '=') {
                // Padding only at the end of the last group, and at most two
                if (!last || j < 2) {
                    return std::nullopt;
                }
                ++padding;
                continue;
            }
            if (padding > 0) {
                return std::nullopt;
            }
            const std::size_t value = Alphabet.find(c);
            if (value == std::string_view::npos) {
                return std::nullopt;
            }
            group |= static_cast<std::uint32_t>(value);
        }
        for (std::size_t j = 0; j < 3 - padding; ++j) {
            result += static_cast<char>((group >> (16U - 8U * j)) & 0xFFU);
        }
    }
    return result;
}

} // namespace rtype::server
