#pragma once

#include <string_view>

namespace rtype::network
{

/// Tells whether a text can be sent as a `str8` field of the protocol: valid UTF-8, without ASCII control
/// characters (`0x00`–`0x1F` and `0x7F`).
///
/// Valid UTF-8 means the shortest encoding of each character (no overlong forms), no UTF-16 surrogates
/// (U+D800–U+DFFF), and nothing above U+10FFFF. The length is not checked: each field sets its own range.
///
/// Use it to check a name typed by the player before sending it.
[[nodiscard]] bool isValidText(std::string_view text);

} // namespace rtype::network
