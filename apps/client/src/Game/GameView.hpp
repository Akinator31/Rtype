#pragma once

#include "rtype/render/Types.hpp"

namespace rtype::client
{

/// The camera of the game scene: an orthographic camera that sees the plane z = 0 as the screen sees the
/// game, a world unit for a game unit, y pointing down. A model at the position of an entity of the game is
/// drawn where its box would be, and keeps its size anywhere on the screen.
/// @param visibleArea The part of the game that the screen shows (render::Renderer::visibleArea()).
[[nodiscard]] render::Camera computeGameView(render::Rectangle visibleArea);

/// The camera of the game looks with y pointing down: a half turn of a model around its length (the x axis)
/// puts its top (+y) up on the screen. The models also turn by a half turn around their vertical axis to face
/// left.
inline constexpr float HalfTurn = 180.0f;
inline constexpr render::Vector3 LengthAxis{1.0f, 0.0f, 0.0f};

/// The ships are seen from the side, and a little from above: this many degrees more than the half turn
/// around their length show their top.
inline constexpr float ShipTilt = 20.0f;

/// The flames of a ship, whatever it does: a little flame, halfway between the Idle pose of the rig (0),
/// whose flames hide in the thrusters, and the Move one (1).
inline constexpr float ShipFlame = 0.5f;

/// The ships are lit from the top left, in front of them, in the world of the game where y points down. A
/// cold rim light from behind, at the top right where the Bydos come from, outlines their hulls against
/// space.
inline constexpr render::Light GameLight{.direction = {0.5f, 0.8f, 0.6f},
                                         .color = {255, 248, 240, 255},
                                         .ambient = {48, 50, 62, 255},
                                         .rimDirection = {-0.6f, 0.35f, -1.0f},
                                         .rim = {95, 135, 210, 255}};

/// A game opened from the take-off of the hangar starts on the shot the hangar ends with, which looks like
/// the game: close up on the ship of the local player, at the same place of the screen. Then the camera pulls
/// back to the whole game (RescueCamera::arrival).
///
/// At first, the camera sees ArrivalZoom of the visible area, the centre of the hull of the ship at
/// ArrivalScreen, as shares of the width and the height of the screen from its top left corner.
inline constexpr float ArrivalZoom = 0.45f;
inline constexpr render::Vector2 ArrivalScreen{0.32f, 0.5f};
/// The time the camera takes to pull back, in seconds.
inline constexpr float ArrivalDuration = 1.8f;

/// The part of the game the camera sees as a game opens from the hangar, the ship at `ship`.
[[nodiscard]] render::Rectangle computeArrivalFrame(render::Rectangle visibleArea, render::Vector2 ship);

} // namespace rtype::client
