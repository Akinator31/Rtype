#pragma once

#include "Console/Console.hpp"

namespace rtype::client
{

/// "profile [on|off]": starts or stops the diagnostics of the frames (FrameProfile), which log, once a
/// second, what the frames took, part by part, and every slow frame. Without an argument, it switches them.
/// @param enabled Whether the frames are profiled, read by the game loop. It must outlive the console.
void registerProfileCommand(Console &console, bool &enabled);

} // namespace rtype::client
