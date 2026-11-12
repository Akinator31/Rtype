#pragma once

#include "rtype/render/Types.hpp"
#include "rtype/xr/Math.hpp"

#include <optional>

/// Where the controllers point in the headset (docs/architecture/vr.md#the-pointer): their rays against the
/// curved panel of the menus. Pure functions, in the space of the headset, in metres.
namespace rtype::client
{

/// A half-line: where it starts, and its direction, of length 1.
struct Ray {
    render::Vector3 origin;
    render::Vector3 direction{0.0f, 0.0f, -1.0f};
};

/// The panel as the pointer meets it: as Renderer::drawImageQuad() draws it.
struct PanelSurface {
    /// The transform of the square of side 1 (panelTransform()).
    render::Matrix transform;
    /// How far its sides come towards the eyes (ImageQuadLook::bend).
    float bend = 0.0f;
    /// Size of the game, in game units: what the panel shows.
    render::Vector2 gameSize;
};

/// Where a ray meets the panel.
struct PanelHit {
    /// The point, in the space of the headset.
    render::Vector3 point;
    /// The same point on the frame drawn on the panel, in game units: (0, 0) at its top left.
    render::Vector2 game;
    /// How far it is from the start of the ray, in metres.
    float distance = 0.0f;
};

/// The ray of a controller: from its aim pose, along its -z.
[[nodiscard]] Ray aimRay(const xr::Pose &aim);

/// Where a ray meets the panel, from its front or its back, if it does: the nearest point ahead of the start
/// of the ray, inside the panel.
[[nodiscard]] std::optional<PanelHit> hitPanel(const Ray &ray, const PanelSurface &panel);

/// The transform of a box of side 1 into a controller held at a grip pose: a small handle along the grip,
/// its front towards where the hand points.
[[nodiscard]] render::Matrix controllerTransform(const xr::Pose &grip);

/// How long the ray of a controller is when it meets nothing, in metres.
inline constexpr float FreeRayLength = 1.2f;

} // namespace rtype::client
