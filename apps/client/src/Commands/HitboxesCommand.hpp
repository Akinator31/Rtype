#pragma once

#include "Console/Console.hpp"

namespace rtype::client
{

/// "hitboxes [on|off]": shows or hides the hitboxes of the entities and the collision grid, like F3. Without
/// an argument, it switches them.
/// @param shown Whether the overlay is drawn, read by the game loop. It must outlive the console.
void registerHitboxesCommand(Console &console, bool &shown);

} // namespace rtype::client
