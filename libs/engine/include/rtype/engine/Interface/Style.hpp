#pragma once

#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstddef>
#include <optional>

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
/// focus, and their sounds. Sizes are in game units.
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

    /// Played when a panel lights up, under the pointer or with the focus, but not when a menu appears with
    /// its first button lit. None by default.
    std::optional<SoundEffect> hoverSound;
    /// Played when a button is pressed, before its action. None by default.
    std::optional<SoundEffect> clickSound;
};

/// Look of the console: a dark band over the game, its lines, and the command line where commands are typed
/// (a TextFieldType::Console field).
struct ConsoleStyle {
    float textSize = 28.0f;
    /// The band behind the console.
    render::Color background{8, 10, 20, 230};
    /// Line along the bottom edge of the band.
    render::Color edge{120, 200, 255, 140};
    /// The lines written in the console.
    render::Color text{200, 210, 225, 255};
    /// Behind the command line.
    render::Color field{255, 255, 255, 16};
    /// The prompt and the cursor of the command line.
    render::Color prompt{120, 200, 255, 255};
    float cursorWidth = 2.0f;
};

/// Look and timing of the toasts (see Engine::showToast()): a dark rounded panel with a coloured accent on
/// its left, then the text on one line, written with the font of the buttons. Toasts are stacked from a
/// corner of the screen, the newest nearest to it. Sizes are in game units, durations in seconds.
struct ToastStyle {
    /// Corner of the screen the toasts are stacked from, and the distance from its edges.
    AnchorPoint corner = AnchorPoint::TopRight;
    float margin = 32.0f;
    /// A toast is wider when its text needs it.
    float minWidth = 420.0f;
    float textSize = 30.0f;
    /// Space around the accent and the text, inside the panel.
    float padding = 18.0f;
    float accentWidth = 6.0f;
    /// Vertical space between two toasts.
    float spacing = 12.0f;
    float cornerRadius = 14.0f;
    float borderThickness = 1.0f;
    /// Shadow cast below the panel: its colour, how far below and how blurred.
    render::Color shadow{0, 0, 0, 110};
    float shadowOffset = 10.0f;
    float shadowBlur = 28.0f;

    render::Color background{14, 18, 32, 230};
    render::Color border{255, 255, 255, 40};
    render::Color text{230, 236, 245, 255};
    /// Colours of the accent, one per ToastType.
    render::Color info{120, 200, 255, 255};
    render::Color success{110, 220, 140, 255};
    render::Color warning{255, 196, 80, 255};
    render::Color error{255, 96, 96, 255};

    /// Time a toast stays fully shown, between its fade in and its fade out.
    float duration = 3.0f;
    float fadeDuration = 0.25f;
    /// How far a toast slides while it fades, from the edge of the screen.
    float slideDistance = 24.0f;
    /// How fast the toasts glide to their new place when another one comes or goes: the part of the distance
    /// left covered per second.
    float glideSpeed = 12.0f;
    /// Beyond it, the oldest toasts fade out early.
    std::size_t maxVisible = 4;

    /// Height of every toast: one line of text and its padding.
    [[nodiscard]] float height() const { return textSize + (2.0f * padding); }

    [[nodiscard]] render::Color getAccent(ToastType type) const
    {
        switch (type) {
        case ToastType::Info:
            return info;
        case ToastType::Success:
            return success;
        case ToastType::Warning:
            return warning;
        case ToastType::Error:
            return error;
        }
        return info;
    }
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
    /// Space between the back button and the title of a header (see InterfaceBuilder::header()).
    float headerSpacing = 24.0f;

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
    ConsoleStyle console{};
    ToastStyle toast{};
};

} // namespace rtype::engine
