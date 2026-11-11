#pragma once

#include "rtype/render/Types.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client
{

/// How long the camera takes to fly to the view of a submenu, or back, in seconds.
inline constexpr float MenuFlightDuration = 2.2f;
/// How long the interface of a menu takes to fade in when the menu comes to the front, in seconds.
inline constexpr float MenuFadeDuration = 0.35f;

/// Where the camera of the main menu goes while a submenu is open: the name of the scene of the submenu, and
/// the view of the 3D world shown behind it.
struct SubmenuView {
    std::string scene;
    render::Camera view;
};

/// The transitions of the camera of the main menu, on the entity of its Camera and of its Intro: when a
/// submenu of `views` opens, the camera flies to its view, and back to the view of the menu when it closes.
/// The scenes opened over a submenu keep its view. Run by the MenuTransitionSystem.
struct MenuCameraFlight {
    std::vector<SubmenuView> views;
    /// The view of `views` the camera flies to or is at; none: the view of the menu.
    std::optional<std::size_t> destination{};
    /// Where the flight started, and the time since, in seconds.
    render::Camera from{};
    float time = MenuFlightDuration;
    /// The view shown at the last update, from which the next flight starts; none before the first one.
    std::optional<render::Camera> shown{};
};

/// Fades an element of the interface in, from invisible to `target`, its opacity before the fade. Removed
/// once the fade is over. Given by the MenuTransitionSystem.
struct MenuFade {
    float target = 1.0f;
    /// Time since the start of the fade, in seconds.
    float time = 0.0f;
};

} // namespace rtype::client
