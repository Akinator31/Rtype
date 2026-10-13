#pragma once

#include "Console/Console.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// Registers every console command of the client. Each command has its own file in Commands/: to add one,
/// write its register function there and call it from here.
/// @param engine, window They must outlive the console.
void registerClientCommands(Console &console, engine::Engine &engine, render::Window &window);

} // namespace rtype::client
