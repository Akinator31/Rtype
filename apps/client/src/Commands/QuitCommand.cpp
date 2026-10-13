#include "Commands/QuitCommand.hpp"

namespace rtype::client
{

void registerQuitCommand(Console &console, render::Window &window)
{
    console.registerConsoleCommand("quit", "quit: closes the game",
                                   [&window](Console & /*target*/, const Console::Arguments & /*arguments*/) {
                                       window.requestClose();
                                   });
}

} // namespace rtype::client
