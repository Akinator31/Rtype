#pragma once

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

/// The animation that moves the small parts of the auxiliary screens.
inline constexpr std::string_view CockpitScreensAnimation = "Screens";
/// How fast it plays: slowly, as instruments, not as a show.
inline constexpr float CockpitScreensSpeed = 0.5f;
/// How many times per second the cockpit is posed while it plays: posing it moves all its vertices on the
/// processor, and screens that refresh 15 times per second look like screens.
inline constexpr float CockpitScreensRate = 15.0f;

} // namespace rtype::client
