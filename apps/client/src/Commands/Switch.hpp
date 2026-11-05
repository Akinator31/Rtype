#pragma once

#include "Console/Console.hpp"

#include <optional>
#include <string_view>

namespace rtype::client
{

/// The state the arguments of a command that shows or hides something ask for: "on", "off", or the opposite
/// of the current state without an argument. Wrong arguments print the usage, and give nothing.
[[nodiscard]] std::optional<bool> readSwitch(Console &console, const Console::Arguments &arguments,
                                             bool current, std::string_view usage);

} // namespace rtype::client
