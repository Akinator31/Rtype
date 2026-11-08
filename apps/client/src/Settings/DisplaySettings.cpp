#include "Settings/DisplaySettings.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>

namespace rtype::client
{

namespace
{

constexpr std::string_view FullscreenName = "fullscreen";

/// A number written alone in the text, above 0.
std::optional<int> parseSize(std::string_view text)
{
    int value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value <= 0) {
        return std::nullopt;
    }
    return value;
}

bool offers(const render::Window &window, Resolution resolution)
{
    const std::vector<Resolution> list = resolutionsOf(window);
    return std::ranges::find(list, resolution) != list.end();
}

} // namespace

std::vector<Resolution> resolutionsFor(const render::Monitor &monitor)
{
    std::vector<Resolution> list{Fullscreen};
    for (const Resolution size : WindowSizes) {
        if (size.width < monitor.width && size.height < monitor.height) {
            list.push_back(size);
        }
    }
    return list;
}

std::string resolutionSettingName(Resolution resolution)
{
    if (resolution.isFullscreen()) {
        return std::string(FullscreenName);
    }
    return std::to_string(resolution.width) + "x" + std::to_string(resolution.height);
}

std::optional<Resolution> parseResolution(std::string_view text)
{
    if (text == FullscreenName) {
        return Fullscreen;
    }
    const std::size_t separator = text.find('x');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const std::optional<int> width = parseSize(text.substr(0, separator));
    const std::optional<int> height = parseSize(text.substr(separator + 1));
    if (!width || !height) {
        return std::nullopt;
    }
    return Resolution{*width, *height};
}

std::vector<Resolution> resolutionsOf(const render::Window &window)
{
    const std::vector<render::Monitor> monitors = window.monitors();
    const auto index = static_cast<std::size_t>(window.monitor());
    return index < monitors.size() ? resolutionsFor(monitors[index]) : std::vector<Resolution>{Fullscreen};
}

Resolution currentResolution(const render::Window &window)
{
    return window.isFullscreen() ? Fullscreen : Resolution{window.width(), window.height()};
}

void setResolution(render::Window &window, Resolution resolution)
{
    if (resolution.isFullscreen()) {
        window.setFullscreen(true);
    } else {
        window.setSize(resolution.width, resolution.height);
    }
}

void moveToNextMonitor(render::Window &window)
{
    moveToMonitor(window, 1);
}

void moveToMonitor(render::Window &window, int direction)
{
    const auto count = static_cast<int>(window.monitors().size());
    if (count < 2) {
        return;
    }
    window.moveToMonitor((window.monitor() + (direction < 0 ? count - 1 : 1)) % count);
    if (!offers(window, currentResolution(window))) {
        window.setFullscreen(true);
    }
}

int readFpsLimit(const engine::Settings &settings)
{
    const int limit = settings.getInt(FpsLimitSettingsKey).value_or(DefaultFpsLimit);
    return limit >= 0 ? limit : DefaultFpsLimit;
}

void applyDisplaySettings(const engine::Settings &settings, render::Window &window)
{
    window.setTargetFps(readFpsLimit(settings));
    window.setVsync(settings.getBool(VsyncSettingsKey).value_or(true));
    if (const std::optional<int> monitor = settings.getInt(MonitorSettingsKey)) {
        window.moveToMonitor(*monitor);
    }
    const std::optional<Resolution> kept =
        parseResolution(settings.getString(ResolutionSettingsKey).value_or(""));
    setResolution(window, kept && offers(window, *kept) ? *kept : Fullscreen);
}

void keepDisplaySettings(engine::Settings &settings, const render::Window &window)
{
    settings.setInt(MonitorSettingsKey, window.monitor());
    settings.setString(ResolutionSettingsKey, resolutionSettingName(currentResolution(window)));
    settings.setInt(FpsLimitSettingsKey, window.targetFps());
    settings.setBool(VsyncSettingsKey, window.isVsync());
}

} // namespace rtype::client
