#include "Commands/Switch.hpp"

#include <string>

namespace rtype::client
{

std::optional<bool> readSwitch(Console &console, const Console::Arguments &arguments, bool current,
                               std::string_view usage)
{
    if (arguments.empty()) {
        return !current;
    }
    if (arguments.size() == 1 && (arguments[0] == "on" || arguments[0] == "off")) {
        return arguments[0] == "on";
    }
    console.print("Usage: " + std::string(usage));
    return std::nullopt;
}

} // namespace rtype::client
