#pragma once

#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

namespace rtype::engine
{

/// One state of a button: its glass panel and the light around it.
struct PanelLook {
    /// Colours of the panel, blended from its left edge to its right edge.
    render::Color left;
    render::Color right;
    render::Color border;
    /// Light spreading around the panel; transparent for none.
    render::Color glow;
};

/// Look of the buttons: a frosted glass panel of a fixed size, which lights up while the button has the
/// focus. Sizes are in game units.
struct ButtonStyle {
    /// Font of the text of the buttons.
    render::FontId font;
    float textSize = 32.0f;
    float width = 476.0f;
    float height = 65.0f;
    float cornerRadius = 23.0f;
    float borderThickness = 1.5f;
    /// How far the glow spreads around the panel.
    float glowSize = 28.0f;
    /// Shadow cast below the panel: its colour, how far below and how blurred.
    render::Color shadow{0, 0, 0, 102};
    float shadowOffset = 18.0f;
    float shadowBlur = 40.0f;
    /// Faint light along the top edge of the panel.
    render::Color topLight{255, 255, 255, 26};
    float topLightOffset = 2.0f;
    float topLightBlur = 12.0f;
    /// Shadow under the text, so that it stands out from the glass.
    render::Color textShadow{0, 0, 0, 128};
    float textShadowOffset = 2.0f;
    /// Time to go from one look to the other, in seconds.
    float fadeDuration = 0.15f;

    PanelLook normal{{255, 255, 255, 102}, {255, 255, 255, 18}, {255, 255, 255, 102}, {191, 224, 255, 0}};
    PanelLook focused{{255, 255, 255, 158}, {217, 237, 255, 51}, {255, 255, 255, 242}, {191, 224, 255, 128}};
};

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
    /// Width of the frame around the focused widget, except buttons, which light up instead.
    float frameThickness = 4.0f;
    /// Scale reached by the focused widget, with a small bounce. Buttons keep their size.
    float focusScale = 1.1f;

    render::Color text = render::colors::White;
    /// Text and frame of the focused widget, except buttons.
    render::Color focused = render::colors::Yellow;
    /// Drawn over the game behind an overlay menu, such as the pause menu.
    render::Color dim{0, 0, 0, 180};
    /// Background of the menus that are not overlays, such as the home screen.
    render::Color background{12, 12, 28, 255};
    /// Background of the selected text in a text field.
    render::Color selection{0, 121, 241, 160};
    ButtonStyle button{};
};

} // namespace rtype::engine
