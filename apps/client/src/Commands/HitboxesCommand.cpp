#include "Commands/HitboxesCommand.hpp"

#include "Commands/Switch.hpp"

namespace rtype::client
{

void registerHitboxesCommand(Console &console, bool &shown)
{
    console.registerConsoleCommand("hitboxes",
                                   "hitboxes [on|off]: shows or hides the hitboxes of the entities (F3)",
                                   [&shown](Console &target, const Console::Arguments &arguments) {
                                       if (const std::optional<bool> wanted =
                                               readSwitch(target, arguments, shown, "hitboxes [on|off]")) {
                                           shown = *wanted;
                                           target.print(shown ? "Hitboxes shown" : "Hitboxes hidden");
                                       }
                                   });
}

} // namespace rtype::client
