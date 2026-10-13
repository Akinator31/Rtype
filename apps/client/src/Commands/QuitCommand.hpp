#pragma once

#include "Console/Console.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// "quit": closes the game.
/// @param window It must outlive the console.
void registerQuitCommand(Console &console, render::Window &window);

} // namespace rtype::client
