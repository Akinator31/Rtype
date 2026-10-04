#pragma once

#include "rtype/render/Types.hpp"

namespace rtype::render
{

/// Area of the window where the game is drawn.
///
/// The game keeps its aspect ratio whatever the window size: it is scaled to the largest rectangle that fits
/// in the window, centred, and the remaining space is left as black bars.
/// @param windowSize Size of the window, in pixels.
/// @param gameSize Logical resolution of the game.
/// @return The area in window pixels, or an empty rectangle if one of the sizes is empty.
[[nodiscard]] Rectangle letterbox(Vector2 windowSize, Vector2 gameSize);

} // namespace rtype::render
