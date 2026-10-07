#pragma once

#include <string_view>

namespace rtype::render
{

/// A filter applied to the colours of the whole image, for accessibility (see Renderer::setColorFilter()).
enum class ColorFilter {
    /// Colours are unchanged.
    None,
    /// Helps players who do not perceive red (protanopia): the colours they confuse are shifted towards
    /// colours they can tell apart.
    Protanopia,
    /// Helps players who do not perceive green (deuteranopia), the most common colour blindness.
    Deuteranopia,
    /// Helps players who do not perceive blue (tritanopia).
    Tritanopia,
    /// Shows the image as a player with protanopia sees it: for the team, to check that the game stays
    /// readable.
    SimulateProtanopia,
    /// Shows the image as a player with deuteranopia sees it.
    SimulateDeuteranopia,
    /// Shows the image as a player with tritanopia sees it.
    SimulateTritanopia,
    /// Stronger contrast and saturation, for players with low vision.
    HighContrast,
};

/// Name of a filter in English, for instance "Deuteranopia" or "Simulate deuteranopia", to show it in a menu.
[[nodiscard]] std::string_view colorFilterName(ColorFilter filter);

} // namespace rtype::render
