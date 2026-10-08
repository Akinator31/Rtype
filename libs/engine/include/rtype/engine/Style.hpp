#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::engine
{

/// Look of the interface: font, sizes and colours, in game units. One style for the whole interface.
struct InterfaceStyle {
    /// Font of every text of the interface.
    render::FontId font;
    float titleSize = 112.0f;
    float textSize = 56.0f;
    /// Space around the text of a widget, inside its frame: also the area the mouse can point at.
    float padding = 16.0f;
    /// Vertical space between two widgets of a column.
    float spacing = 16.0f;
    /// Width of the frame around the focused widget.
    float frameThickness = 4.0f;
    /// Scale reached by the focused widget, with a small bounce.
    float focusScale = 1.1f;

    render::Color text = render::colors::White;
    /// Text and frame of the focused widget.
    render::Color focused = render::colors::Yellow;
    /// Drawn over the game behind an overlay menu, such as the pause menu.
    render::Color dim{0, 0, 0, 180};
    /// Background of the menus that are not overlays, such as the home screen.
    render::Color background{12, 12, 28, 255};
    /// Background of the selected text in a text field.
    render::Color selection{0, 121, 241, 160};
};

} // namespace rtype::engine
