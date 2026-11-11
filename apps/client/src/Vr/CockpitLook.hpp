#pragma once

#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/render/Types.hpp"

#include <array>
#include <string_view>

/// The cockpit the player sits in, in a VR headset (RTYPE_MODEL_COCKPIT, docs/architecture/vr.md), measured
/// on the model: in its space, in metres, y up, its nose towards +x and the right of the pilot towards +z.
namespace rtype::client
{

/// Where the eyes of the pilot are: 9 cm in front of the headrest of Cockpit_Seat, a little under its top,
/// 73 cm over the cushion. The origin of the space of the headset, the head of the player, is put there.
inline constexpr render::Vector3 CockpitEyePoint{-0.17f, 0.05f, 0.0f};

/// The highest points of what is in front of the pilot, which the bottom of the board must stay over: the
/// nose of the hull under the canopy, and its sides. The arch around the hologram and the struts of the
/// canopy are thin, and seen through.
inline constexpr std::array<render::Vector3, 3> CockpitDashboardTop{{
    {1.32f, -0.28f, 0.0f},
    {1.30f, -0.247f, 0.42f},
    {1.30f, -0.247f, -0.42f},
}};

/// The transparent meshes, drawn after the others from the farthest to the nearest: the canopy and the
/// hologram over the dashboard.
inline constexpr std::array<std::string_view, 2> CockpitTransparentMeshes{"Cockpit_Glass", "Cockpit_Holo"};

/// The centre of the hologram over the dashboard (Cockpit_Holo), where the panel of the menus unfolds from.
inline constexpr render::Vector3 CockpitHoloCentre{0.66f, -0.10f, 0.0f};

/// A screen of the cockpit that shows a surface of the interface (Figma, Série 17): the emission map of its
/// material is replaced by the image the surface is drawn into. The texture coordinates of the meshes of
/// these materials cover their whole map, but the panel of the dashboard hides the sides of some screens
/// from the eye point: the surface is drawn only into the part of the image the pilot sees (`shown`).
struct CockpitScreen {
    engine::Surface surface;
    /// The material of the screen in the model.
    std::string_view material;
    /// The area of the surface, in game units, where the anchors place its entities: of the shape of the part
    /// of the screen the pilot sees, so that nothing is stretched nor hidden.
    render::Vector2 size;
    /// The image it is drawn into, in pixels: of the shape of the whole screen, about 4/3 of a pixel per game
    /// unit.
    render::Vector2 resolution;
    /// The part of the image the pilot sees from the eye point, in fractions of the image from its top left:
    /// measured on the model by rendering the view of the pilot with bands every 5 % of the image.
    render::Rectangle shown{0.0f, 0.0f, 1.0f, 1.0f};
};

/// The screens of the cockpit, and what they show: the ship on the right, the wave and the session in the
/// middle, the other players on the small screens of the sides (the four share the image), the radar on the
/// hologram. The panel hides the sides of the screen of the ship (17 % each) and of the navigation (26 %
/// each): what is left of them is about square, of the screen of the navigation even taller than wide. The
/// flight stick hides the bottom right of the screen of the ship, whose HUD stays on its left.
inline constexpr std::array<CockpitScreen, 4> CockpitScreens{{
    {engine::Surface::HudShip,
     "CP_ScreenShip",
     {317.0f, 360.0f},
     {640.0f, 480.0f},
     {0.17f, 0.0f, 0.66f, 1.0f}},
    {engine::Surface::HudNav, "CP_ScreenNav", {230.0f, 310.0f}, {640.0f, 412.0f}, {0.26f, 0.0f, 0.48f, 1.0f}},
    {engine::Surface::HudAux, "CP_ScreenAux", {420.0f, 300.0f}, {560.0f, 400.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
    {engine::Surface::Holo, "CP_Holo", {480.0f, 330.0f}, {640.0f, 440.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
}};

/// The area of the game a frame of a screen shows (render::Renderer::beginFrame(image, area, ...)): the whole
/// image, so that the area of the surface, from (0, 0) to its size, lands on the part the pilot sees.
[[nodiscard]] constexpr render::Rectangle screenFrameArea(const CockpitScreen &screen)
{
    const float width = screen.size.x / screen.shown.width;
    const float height = screen.size.y / screen.shown.height;
    return {-screen.shown.x * width, -screen.shown.y * height, width, height};
}

/// The animation that moves the small parts of the auxiliary screens.
inline constexpr std::string_view CockpitScreensAnimation = "Screens";
/// How fast it plays: slowly, as instruments, not as a show.
inline constexpr float CockpitScreensSpeed = 0.5f;
/// How many times per second the cockpit is posed while it plays: posing it moves all its vertices on the
/// processor, and screens that refresh 15 times per second look like screens.
inline constexpr float CockpitScreensRate = 15.0f;

} // namespace rtype::client
