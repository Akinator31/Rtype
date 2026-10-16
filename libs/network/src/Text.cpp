#include "rtype/network/Text.hpp"

#include <cstddef>
#include <cstdint>

namespace rtype::network
{

bool isValidText(std::string_view text)
{
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[i]);
        if (lead < 0x80) {
            if (lead < 0x20 || lead == 0x7F) {
                return false; // control character
            }
            ++i;
            continue;
        }

        // Number of continuation bytes, and the smallest value this length may encode (no overlong forms)
        std::size_t continuation = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((lead & 0xE0) == 0xC0) {
            continuation = 1;
            codePoint = lead & 0x1F;
            minimum = 0x80;
        } else if ((lead & 0xF0) == 0xE0) {
            continuation = 2;
            codePoint = lead & 0x0F;
            minimum = 0x800;
        } else if ((lead & 0xF8) == 0xF0) {
            continuation = 3;
            codePoint = lead & 0x07;
            minimum = 0x10000;
        } else {
            return false; // a continuation byte without a lead byte, or 0xF8–0xFF
        }
        if (text.size() - i - 1 < continuation) {
            return false; // cut in the middle of a character
        }
        for (std::size_t k = 1; k <= continuation; ++k) {
            const auto next = static_cast<std::uint8_t>(text[i + k]);
            if ((next & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }
        if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return false;
        }
        // Control characters of the C1 range (U+0080–U+009F) are allowed: only ASCII controls are refused
        i += continuation + 1;
    }
    return true;
}

} // namespace rtype::network
