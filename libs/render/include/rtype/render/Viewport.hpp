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

/// Converts a position in the window, for instance the mouse pointer, into game units: the reverse of the
/// scaling done by letterbox().
///
/// A position in the black bars gives coordinates outside of the game, for instance negative ones: check that
/// they are between 0 and the game size before using them.
/// @param position Position in the window, in pixels.
/// @param windowSize Size of the window, in pixels.
/// @param gameSize Logical resolution of the game.
/// @return The position in game units, or (0, 0) if one of the sizes is empty.
[[nodiscard]] Vector2 windowToGame(Vector2 position, Vector2 windowSize, Vector2 gameSize);

} // namespace rtype::render
