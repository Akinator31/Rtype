#pragma once

#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/render/Window.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rtype::client
{

/// Where the screen and the resolution chosen on the Settings page are kept in the settings of the engine.
inline constexpr std::string_view MonitorSettingsKey = "display.monitor";
inline constexpr std::string_view ResolutionSettingsKey = "display.resolution";

/// A size of the window in pixels, or fullscreen: a width and a height of 0. Fullscreen covers the screen at
/// its own resolution (see render::Window::setFullscreen()).
struct Resolution {
    int width = 0;
    int height = 0;

    [[nodiscard]] bool isFullscreen() const { return width == 0; }
    bool operator==(const Resolution &) const = default;
};

inline constexpr Resolution Fullscreen{};

/// The sizes a window can be given on the Settings page, from the smallest.
inline constexpr std::array<Resolution, 5> WindowSizes{
    {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}}};

/// The resolutions offered on a screen: fullscreen, then the WindowSizes smaller than the screen, since a
/// window as large as its screen would not fit with its borders.
[[nodiscard]] std::vector<Resolution> resolutionsFor(const render::Monitor &monitor);

/// How a resolution is written in the settings: "fullscreen" or "1600x900".
[[nodiscard]] std::string resolutionSettingName(Resolution resolution);
/// The resolution written this way, or nothing for a text that is not one.
[[nodiscard]] std::optional<Resolution> parseResolution(std::string_view text);

/// The resolutions offered on the screen of the window.
[[nodiscard]] std::vector<Resolution> resolutionsOf(const render::Window &window);
/// The resolution of the window now: fullscreen, or its size.
[[nodiscard]] Resolution currentResolution(const render::Window &window);
/// Puts the window in fullscreen, or gives it this size.
void setResolution(render::Window &window, Resolution resolution);
/// Moves the window to the next screen, back to the first after the last. A window too large for the new
/// screen goes fullscreen.
void moveToNextMonitor(render::Window &window);

/// Moves the window to the screen kept in the settings, and gives it the resolution kept if that screen
/// offers it. A screen that is no longer connected leaves the window where it is; no resolution, a wrong one
/// or one too large leaves fullscreen.
void applyDisplaySettings(const engine::Settings &settings, render::Window &window);
/// Keeps the screen and the resolution of the window in the settings, without writing them.
void keepDisplaySettings(engine::Settings &settings, const render::Window &window);

} // namespace rtype::client
