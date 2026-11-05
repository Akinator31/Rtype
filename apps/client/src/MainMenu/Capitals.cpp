#include "MainMenu/Capitals.hpp"

namespace rtype::client
{

std::string toCapitals(std::string text)
{
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte >= 'a' && byte <= 'z') {
            text[index] = static_cast<char>(byte - ('a' - 'A'));
        } else if (byte == 0xC3 && index + 1 < text.size()) {
            // é is written in UTF-8 as C3 A9, É as C3 89
            const auto next = static_cast<unsigned char>(text[index + 1]);
            if (next >= 0xA0 && next <= 0xBE && next != 0xB7) {
                text[index + 1] = static_cast<char>(next - 0x20);
            }
            ++index;
        }
    }
    return text;
}

} // namespace rtype::client
