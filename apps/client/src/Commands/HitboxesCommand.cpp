#include "Commands/HitboxesCommand.hpp"

#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

/// The state asked by the arguments: none for a switch, or if they are wrong.
std::optional<bool> readWantedState(Console &console, const Console::Arguments &arguments, bool shown)
{
    if (arguments.empty()) {
        return !shown;
    }
    if (arguments.size() == 1 && arguments[0] == "on") {
        return true;
    }
    if (arguments.size() == 1 && arguments[0] == "off") {
        return false;
    }
    console.print("Usage: hitboxes [on|off]");
    return std::nullopt;
}

void showHitboxes(Console &console, const Console::Arguments &arguments, bool &shown)
{
    if (const std::optional<bool> wanted = readWantedState(console, arguments, shown)) {
        shown = *wanted;
        console.print(shown ? "Hitboxes shown" : "Hitboxes hidden");
    }
}

} // namespace

void registerHitboxesCommand(Console &console, bool &shown)
{
    console.registerConsoleCommand("hitboxes",
                                   "hitboxes [on|off]: shows or hides the hitboxes of the entities (F3)",
                                   [&shown](Console &target, const Console::Arguments &arguments) {
                                       showHitboxes(target, arguments, shown);
                                   });
}

} // namespace rtype::client
