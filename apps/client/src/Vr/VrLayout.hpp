#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/xr/Math.hpp"

#include <array>
#include <span>
#include <string_view>
#include <vector>

/// Where things are in the headset (docs/architecture/vr.md#the-scene-in-the-headset): the space of the
/// headset has its origin at the head of the player, the eye point of the cockpit, in metres, y up, -z ahead.
namespace rtype::client
{

/// Where the board, the frame of the game, floats outside the canopy: chosen so that the whole board is seen
/// without turning the head (about 67 degrees wide) and its bottom is not hidden by the dashboard. It is just
/// outside the canopy, whose nose is 1.5 m ahead of the eyes, near enough to be felt close. Its centre is
/// over the eyes, the player looks a little up at it, and it is tilted to face them.
struct VrLayout {
    /// Its width, in metres; its height follows the shape of the game.
    float boardWidth = 2.7f;
    /// How far ahead of the eyes its centre is, in metres.
    float boardDistance = 2.0f;
    /// How far its centre is over the eyes, in metres.
    float boardRise = 0.4f;
};

/// The resolution of the image of the board: about 38 pixels per degree across its 67 degrees, more than a
/// headset shows (20 to 25), so that the board stays sharp.
inline constexpr render::Vector2 BoardResolution{2560.0f, 1440.0f};

/// The transform of the board, for Renderer::drawImageQuad(): the square of side 1 around the origin scaled
/// to the board, tilted to face the eyes and moved ahead of them.
/// @param gameSize Size of the game, in game units: the shape of the board.
[[nodiscard]] render::Matrix boardTransform(const VrLayout &layout, render::Vector2 gameSize);

/// The corners of the board in the space of the headset: top left, top right, bottom right, bottom left.
[[nodiscard]] std::array<render::Vector3, 4> boardCorners(const VrLayout &layout, render::Vector2 gameSize);

/// Whether an eye at the origin, looking ahead (-z), sees a point of the space of the headset.
[[nodiscard]] bool isSeen(render::Vector3 point, const xr::FieldOfView &fieldOfView);

/// How far above the horizon of the eyes a point of the space of the headset is seen, in degrees: negative
/// under it.
[[nodiscard]] float elevation(render::Vector3 point);

/// Where the cockpit is drawn so that its eye point is at the origin of the space of the headset, its nose
/// ahead: turned around y, then moved, as Renderer::drawModel() places a model.
struct CockpitPlacement {
    render::Vector3 position;
    render::Vector3 rotationAxis{0.0f, 1.0f, 0.0f};
    /// In degrees.
    float rotationAngle = 0.0f;
};

/// The placement of the cockpit around the eyes.
/// @param eyePoint Where the eyes are in the cockpit, in its space (CockpitEyePoint).
[[nodiscard]] CockpitPlacement placeCockpit(render::Vector3 eyePoint);

/// A point of the cockpit, in its space, moved into the space of the headset by placeCockpit().
[[nodiscard]] render::Vector3 cockpitToHeadset(render::Vector3 point, render::Vector3 eyePoint);

/// The meshes of a model with one of these names, by index, from the farthest from the eye point to the
/// nearest: the order to draw transparent meshes in, so that the nearer ones blend over the farther ones.
[[nodiscard]] std::vector<int> farToNear(const std::vector<render::ModelMesh> &meshes,
                                         std::span<const std::string_view> names, render::Vector3 eyePoint);

} // namespace rtype::client
