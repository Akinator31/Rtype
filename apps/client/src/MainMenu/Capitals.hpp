#pragma once

#include <string>

namespace rtype::client
{

/// The text in capitals, like the buttons and the titles of the design: the letters of ASCII and the accented
/// letters of Latin-1.
[[nodiscard]] std::string toCapitals(std::string text);

} // namespace rtype::client
