#pragma once

#include "rtype/render/Types.hpp"

namespace rtype::render
{

/// Part of the game shown in the window, in game units.
///
/// The game always shows its whole area, from (0, 0) to gameSize, scaled as much as the window allows. When
/// the window has another shape, the view extends around that area, on both sides, so that it fills the
/// window without black bars: a 16:10 screen shows a 1920x1080 game from y = -60 to y = 1140. Game
/// coordinates stay the same on every screen; only what is around the game area changes.
/// @param windowSize Size of the window, in pixels.
/// @param gameSize Size of the game, in game units.
/// @return The visible area, in game units, or the game area if one of the sizes is empty, such as a
/// minimised window.
[[nodiscard]] Rectangle computeVisibleArea(Vector2 windowSize, Vector2 gameSize);

/// Converts a position in the window, for instance the mouse pointer, into game units: the reverse of the
/// scaling of computeVisibleArea().
///
/// A position near the edges of a window that is not of the shape of the game can be outside of the game
/// area, for instance negative: check that it is between 0 and the game size before using it for gameplay.
/// @param position Position in the window, in pixels.
/// @param windowSize Size of the window, in pixels.
/// @param gameSize Size of the game, in game units.
/// @return The position in game units, or (0, 0) if one of the sizes is empty.
[[nodiscard]] Vector2 windowToGame(Vector2 position, Vector2 windowSize, Vector2 gameSize);

} // namespace rtype::render
