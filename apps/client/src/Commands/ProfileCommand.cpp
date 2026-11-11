#include "Commands/ProfileCommand.hpp"

#include "Commands/Switch.hpp"

namespace rtype::client
{

void registerProfileCommand(Console &console, bool &enabled)
{
    console.registerConsoleCommand(
        "profile", "profile [on|off]: logs what the frames take, and the slow ones",
        [&enabled](Console &target, const Console::Arguments &arguments) {
            if (const std::optional<bool> wanted =
                    readSwitch(target, arguments, enabled, "profile [on|off]")) {
                enabled = *wanted;
                target.print(enabled ? "Frames profiled, in the log" : "Frames no longer profiled");
            }
        });
}

} // namespace rtype::client
