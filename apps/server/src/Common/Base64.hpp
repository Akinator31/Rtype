#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rtype::server
{

/// Encodes bytes in base64 (RFC 4648), without line breaks.
[[nodiscard]] std::string base64(std::string_view bytes);

/// Decodes base64 (RFC 4648), its padding required.
/// @return Nothing if the text is not base64: another character, a length that is not a multiple of 4, or
/// padding anywhere but at the end.
[[nodiscard]] std::optional<std::string> decodeBase64(std::string_view text);

} // namespace rtype::server
