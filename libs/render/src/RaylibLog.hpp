#pragma once

// Private to the render library: sends raylib's messages to our logger.

namespace rtype::render::detail
{

/// Redirects raylib's messages to the "raylib" logger. Call it before the window is opened, so that the
/// messages printed while opening it are redirected too.
void redirectRaylibLogs();

} // namespace rtype::render::detail
