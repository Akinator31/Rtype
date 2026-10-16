#pragma once

#include "Console/Console.hpp"
#include "rtype/engine/Engine/Engine.hpp"

namespace rtype::client
{

/// "toast [info|success|warning|error] <text>": shows a toast, of type info when none is given.
/// @param engine It must outlive the console.
void registerToastCommand(Console &console, engine::Engine &engine);

} // namespace rtype::client
