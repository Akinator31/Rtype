#pragma once

#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/render/Renderer.hpp"
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

    /// The panel of the menus, between the dashboard and the board: its width, in metres; its height follows
    /// the shape of the game. About 72 degrees wide, read without moving the head.
    float panelWidth = 1.6f;
    /// How far ahead of the eyes its centre is, and how far over them, in metres.
    float panelDistance = 1.1f;
    float panelRise = 0.05f;
    /// How far its sides come towards the eyes, in metres: a pane curved around the player.
    float panelBend = 0.2f;
    /// Where it unfolds from, in the space of the headset: the hologram over the dashboard
    /// (CockpitHoloCentre).
    render::Vector3 panelOrigin{0.0f, -0.15f, -0.83f};
};

/// How long the panel takes to unfold, or to fold back, in seconds.
inline constexpr float PanelUnfoldTime = 0.25f;
/// How bright the board stays behind the open panel during a game, from 0 to 1: the pause, the death screen
/// and the results are read over the battle. In the menus, the board is not dimmed: the menus are over the
/// world of their scene (the planet of the main menu, the hangar), as on the screen.
inline constexpr float BoardBehindPanel = 0.35f;
/// The resolution of the image of the panel, of the shape of the game.
inline constexpr render::Vector2 PanelResolution{1920.0f, 1080.0f};

/// The area of each surface of the interface in the headset, in game units: the game area for the panel and
/// the board, which have its shape, and the size of each screen of the cockpit (CockpitScreens) from (0, 0).
[[nodiscard]] engine::SurfaceAreas surfaceAreas(render::Vector2 gameSize);

/// The transform of the panel, for Renderer::drawImageQuad(), while it unfolds: from 0 (folded, a point of
/// light on the hologram) to 1 (open). A line of light rises from the hologram to the place of the panel
/// during the first 40 percent, then opens in height and in width.
/// @param gameSize Size of the game, in game units: the shape of the panel.
[[nodiscard]] render::Matrix panelTransform(const VrLayout &layout, render::Vector2 gameSize, float openness);

/// How the panel looks while it unfolds: curved once it opens, over everything, nothing of the cockpit
/// hides it.
/// @param brightness From 0 (black) to 1, the fade between two scenes.
[[nodiscard]] render::ImageQuadLook panelLook(const VrLayout &layout, float openness, float brightness);

/// How bright the board is: dimmed behind the panel as it opens during a game (`playing`), and by the fade
/// between two scenes.
[[nodiscard]] float boardBrightness(float openness, float brightness, bool playing);

/// The resolution of the image of the board: about 38 pixels per degree across its 67 degrees, more than a
/// headset shows (20 to 25), so that the board stays sharp.
inline constexpr render::Vector2 BoardResolution{2560.0f, 1440.0f};

/// The transform of the board, for Renderer::drawImageQuad(): the square of side 1 around the origin scaled
/// to the board, tilted to face the eyes and moved ahead of them.
/// @param gameSize Size of the game, in game units: the shape of the board.
[[nodiscard]] render::Matrix boardTransform(const VrLayout &layout, render::Vector2 gameSize);

/// The corners of the board in the space of the headset: top left, top right, bottom right, bottom left.
[[nodiscard]] std::array<render::Vector3, 4> boardCorners(const VrLayout &layout, render::Vector2 gameSize);

/// The corners of the panel, open, in the space of the headset, without its curve: top left, top right,
/// bottom right, bottom left.
[[nodiscard]] std::array<render::Vector3, 4> panelCorners(const VrLayout &layout, render::Vector2 gameSize);

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
