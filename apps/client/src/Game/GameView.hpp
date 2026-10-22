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

} // namespace rtype::client
