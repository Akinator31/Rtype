#pragma once

#include "rtype/render/Types.hpp"

/// How the camera of the main menu flies from a view to another: for its intro and for its submenus.
namespace rtype::client
{

/// Progress of a flight, from 0 to 1, for a time from 0 to 1: it starts slowly, speeds up, then slows down
/// until it stops, so that the camera neither jumps nor brakes suddenly. The time is clamped.
[[nodiscard]] float easeInOut(float time);

/// The view between two others: the camera moves in a straight line and turns smoothly from one direction
/// to the other. At 0 it is `from`, at 1 `to`.
[[nodiscard]] render::Camera mixViews(const render::Camera &from, const render::Camera &to, float amount);

/// The view from a place of the world towards a point of it, with the field of view of `view`. Looking
/// straight at the point puts it in the middle of the screen; the view is then turned by `turn` degrees to
/// the right, which puts the point on the left of the middle (and on its right for a turn to the left).
[[nodiscard]] render::Camera lookAtView(const render::Camera &view, render::Vector3 position,
                                        render::Vector3 point, float turn);

/// The view from `distance` away from a point of the world, on the line from that point to the camera of
/// `view`: a camera that flies there goes straight towards the point, and stops before it. See lookAtView()
/// for `turn`.
[[nodiscard]] render::Camera approachView(const render::Camera &view, render::Vector3 point, float distance,
                                          float turn);

} // namespace rtype::client
