#pragma once

#include "Console/Console.hpp"
#include "rtype/render/Renderer.hpp"

namespace rtype::client
{

/// "fps [on|off]": shows or hides the number of frames per second, in the top right corner of the screen.
/// Without an argument, it switches it.
/// @param shown Whether the counter is drawn, read by the game loop. It must outlive the console.
void registerFpsCommand(Console &console, bool &shown);

/// Draws the number of frames per second in the top right corner of the screen. Call it in a frame, after
/// the interface, so that it is on top.
void drawFpsCounter(render::Renderer &renderer);

} // namespace rtype::client
