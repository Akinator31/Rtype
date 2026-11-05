#pragma once

#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Component/Toast.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace rtype::engine
{

/// The font of the console, and the sounds of the buttons. How a button looks is FlareStyle.
struct ButtonStyle {
    /// Font of the console.
    render::FontId font;
    /// Played when a button lights up, under the pointer or with the focus, but not when a menu appears with
    /// its first button lit. None by default.
    std::optional<SoundEffect> hoverSound;
    /// Played when a button is pressed, before its action. None by default.
    std::optional<SoundEffect> clickSound;
};

/// Look of the buttons: text alone, with spaced letters. When the button lights up, a
/// line of light traces itself across the text and a star flares after it; when it is pressed, the star
/// bursts and a pulse of light runs along the line, which shines brighter. The sounds are those of
/// ButtonStyle. Sizes are in game units, durations in seconds.
struct FlareStyle {
    render::FontId font;
    float textSize = 30.0f;
    /// Space added between two letters.
    float letterSpacing = 12.0f;
    /// The area the mouse points at: the text and the star, with this space around them, as high as `height`.
    float padding = 12.0f;
    float height = 56.0f;
    /// The text at rest: bright enough to read on a dark sky, dimmer than when lit.
    render::Color text{255, 255, 255, 200};
    render::Color litText = render::colors::White;
    /// Halo behind the lit text.
    render::Color textGlow{255, 255, 255, 24};
    float textGlowBlur = 30.0f;

    /// The line starts this far before the text, fades in until the middle of the text, and ends on the
    /// star.
    render::Color line{255, 255, 255, 170};
    float lineThickness = 1.5f;
    float lineBefore = 150.0f;
    /// Light spreading around the line, which grows with it when the button is pressed.
    render::Color lineGlow{255, 255, 255, 40};
    float lineGlowBlur = 8.0f;

    /// A four-pointed star, this far after the text, its points this long.
    float starGap = 18.0f;
    float starSize = 9.0f;
    float starThickness = 2.0f;

    /// Time for the line to trace itself when the button lights up, and to go out when it is left.
    float traceDuration = 0.4f;
    float fadeDuration = 0.15f;

    /// Highlight reached when the button is pressed, and the time it takes to come back to 1. The action of
    /// the button runs pressDelay after the press, once the burst has been seen.
    float pressedHighlight = 2.0f;
    float pressDuration = 0.45f;
    float pressDelay = 0.3f;
    /// The star grows by burstScale and turns by burstTurn degrees when pressed.
    float burstScale = 2.6f;
    float burstTurn = 90.0f;
    /// The pulse of light that runs from the star to the far end of the line.
    float pulseLength = 70.0f;
    /// The colour of a button that deletes (Panel::danger), for its text, its line and its star.
    render::Color danger{232, 114, 110, 255};

    /// Width of a button whose text is this wide.
    [[nodiscard]] float computeWidth(float textWidth) const
    {
        return padding + textWidth + starGap + (2.0f * starSize) + padding;
    }
};

/// Look of the console, like the rest of the interface: a dark veil over the game with no frame, a fine line
/// of light along its bottom edge, grey lines of text, and above the command line (a TextFieldType::Console
/// field) a fine rule. Sizes are in game units.
struct ConsoleStyle {
    float textSize = 26.0f;
    float margin = 48.0f;
    render::Color background{0, 0, 0, 215};
    render::Color edge{255, 255, 255, 170};
    render::Color edgeGlow{255, 255, 255, 28};
    float edgeThickness = 1.5f;
    float edgeGlowThickness = 8.0f;
    render::Color text{138, 144, 151, 255};
    render::Color command{255, 255, 255, 255};
    render::Color rule{46, 49, 53, 255};
    float ruleThickness = 2.0f;
    float ruleGap = 14.0f;
    render::Color prompt{255, 255, 255, 255};
    float cursorWidth = 2.0f;
};

/// Look and timing of the toasts (see Engine::showToast()): no panel, a line of text on the edge of the
/// screen, written with light: the subject in bright capitals, the message in grey, the actor in bright
/// capitals, and an icon at the end. A toast with a message alone shows it in bright capitals. Toasts are
/// stacked from a corner of the screen, the newest nearest to it, and aligned on the edge of this corner.
/// Sizes are in game units, durations in seconds.
struct ToastStyle {
    /// Corner of the screen the toasts are stacked from, and the distance from its edges.
    AnchorPoint corner = AnchorPoint::TopRight;
    float margin = 48.0f;
    /// Font of the subject and the actor, and of a message shown alone.
    render::FontId nameFont;
    /// Font of the message that follows a subject.
    render::FontId messageFont;
    float nameSize = 24.0f;
    float messageSize = 25.0f;
    /// Space added between two letters of the capitals.
    float letterSpacing = 2.5f;
    /// Space between the parts of a toast: subject, message, actor and icon.
    float gap = 18.0f;
    /// Width and height of the icon.
    float iconSize = 26.0f;
    float iconThickness = 2.0f;
    /// Vertical space between two toasts.
    float spacing = 22.0f;

    /// Bright text, with the light around it.
    render::Color text{255, 255, 255, 255};
    render::Color glow{255, 255, 255, 128};
    float glowBlur = 8.0f;
    /// The message that follows a subject.
    render::Color message{138, 144, 151, 255};
    /// Colours of the capitals and of the icon, one per ToastType.
    render::Color info{255, 255, 255, 255};
    render::Color success{255, 255, 255, 255};
    render::Color warning{232, 114, 110, 255};
    render::Color error{232, 114, 110, 255};

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

    /// An announcement (ToastKind::Announcement): its icon and title in bright capitals, a line of light
    /// under them, and a line of detail in grey. It is centred at the top of the screen.
    float announcementTitleSize = 30.0f;
    float announcementTitleSpacing = 6.5f;
    float announcementDetailSize = 26.0f;
    /// Space between the title and the line, and between the line and the detail.
    float announcementGap = 16.0f;
    float announcementLineWidth = 640.0f;
    float announcementLineThickness = 2.0f;
    /// Distance from the top of the screen.
    float announcementTop = 96.0f;
    /// Time an announcement takes to appear and to leave, in seconds: its icon pops, its title tightens its
    /// letters, its line of light draws itself from the middle, then its detail rises.
    float announcementIn = 0.8f;
    float announcementOut = 0.45f;
    std::size_t maxAnnouncements = 3;

    /// A banner (ToastKind::Banner): a band across the whole screen, between two lines of light, with a veil
    /// of light between them, its title in large capitals with a spark on each side and its detail under it.
    float bannerHeight = 170.0f;
    float bannerTitleSize = 48.0f;
    float bannerTitleSpacing = 16.0f;
    float bannerDetailSize = 24.0f;
    float bannerDetailSpacing = 6.5f;
    float bannerLineThickness = 2.0f;
    /// Strength of the veil, in the middle of the band.
    float bannerVeil = 0.07f;
    /// Size of the sparks beside the title, and their distance from it.
    float bannerSparkSize = 26.0f;
    float bannerSparkGap = 34.0f;
    /// Place of the banner, as a part of the height of the screen, from its top to the middle of the band.
    float bannerCentre = 0.3f;
    /// Time a banner takes to appear and to leave, in seconds: a line of light draws itself from the middle,
    /// the band opens around it, the sparks fly out, the title tightens its letters, then the detail rises.
    float bannerIn = 1.1f;
    float bannerOut = 0.5f;
    /// How much the letters of a title are spaced when it starts to appear, as a multiple of their spacing.
    float spacingIn = 2.6f;
    std::size_t maxBanners = 1;

    /// Height of a line of text.
    [[nodiscard]] float height() const { return std::max(nameSize, std::max(messageSize, iconSize)); }
    /// Height of a toast of this kind.
    [[nodiscard]] float getHeight(ToastKind kind) const
    {
        // One height per kind, in the order of ToastKind
        const std::array<float, toastKindCount> heights{
            height(),
            announcementTitleSize + announcementGap + announcementLineThickness + announcementGap +
                announcementDetailSize,
            bannerHeight,
        };
        return heights[static_cast<std::size_t>(kind)];
    }
    /// Time a toast of this kind takes to appear, and to leave.
    [[nodiscard]] float getFadeIn(ToastKind kind) const
    {
        const std::array<float, toastKindCount> times{fadeDuration, announcementIn, bannerIn};
        return times[static_cast<std::size_t>(kind)];
    }
    [[nodiscard]] float getFadeOut(ToastKind kind) const
    {
        const std::array<float, toastKindCount> times{fadeDuration, announcementOut, bannerOut};
        return times[static_cast<std::size_t>(kind)];
    }
    /// Time from the appearance of a toast of this kind to the end of its exit.
    [[nodiscard]] float getLifetime(ToastKind kind) const
    {
        return getFadeIn(kind) + duration + getFadeOut(kind);
    }
    /// How many toasts of this kind are shown at once.
    [[nodiscard]] std::size_t getMaxVisible(ToastKind kind) const
    {
        const std::array<std::size_t, toastKindCount> maximums{maxVisible, maxAnnouncements, maxBanners};
        return maximums[static_cast<std::size_t>(kind)];
    }

    [[nodiscard]] render::Color getAccent(ToastType type) const
    {
        // One colour per type, in the order of ToastType
        const std::array<render::Color, toastTypeCount> accents{info, success, warning, error};
        return accents[static_cast<std::size_t>(type)];
    }
};

/// Look of the titles (InterfaceBuilder::title()): capitals in chrome, lit from above and below with a dark
/// band between, and a light around them.
struct TitleStyle {
    render::FontId font;
    /// Space added between two letters, as a part of the size of the text.
    float letterSpacing = 0.025f;
    /// Colour from the top of the line to the bottom of it.
    std::vector<render::ColorStop> chrome{{0.0f, {255, 255, 255, 255}},
                                          {0.42f, {208, 213, 218, 255}},
                                          {0.52f, {128, 134, 140, 255}},
                                          {0.64f, {182, 187, 193, 255}},
                                          {1.0f, {244, 246, 248, 255}}};
    /// Light around the text, spreading this far: faint, so that the letters stay sharp.
    render::Color glow{255, 255, 255, 70};
    float glowBlur = 10.0f;
};

/// Look of the sliders (InterfaceBuilder::slider()): a fine rail, its filled part in light, and a ring open
/// on its sides for the handle, which grows when pointed at and when dragged. Sizes are in game units.
struct SliderStyle {
    render::FontId font;
    float captionSize = 24.0f;
    float letterSpacing = 3.0f;
    float width = 560.0f;
    /// Space between the caption and the rail.
    float captionGap = 14.0f;
    float railThickness = 2.0f;
    float fillThickness = 3.0f;
    render::Color rail{46, 49, 53, 255};
    render::Color fill{255, 255, 255, 217};
    /// Light along the filled part: two fainter lines around it, stronger when pointed at and dragged.
    render::Color fillGlow{255, 255, 255, 22};
    /// Radius of the handle at rest, pointed at and dragged, and the thickness of its ring.
    float handleRadius = 16.0f;
    float handleHoverRadius = 19.0f;
    float handleDragRadius = 22.0f;
    float handleThickness = 3.0f;
    /// Angle of the opening of the ring on each side, in degrees.
    float handleOpening = 42.0f;
    render::Color handle{255, 255, 255, 255};
    /// Soft light around the handle, spreading this far, when pointed at and dragged.
    render::Color handleGlow{255, 255, 255, 56};
    float handleGlowSpread = 16.0f;
    /// A faint ring around the handle when it is pointed at, this far from it.
    render::Color handleRing{255, 255, 255, 90};
    float handleRingGap = 7.0f;
    render::Color caption{138, 144, 151, 255};
    render::Color litCaption{255, 255, 255, 255};
    /// Time to go from one look to another, in seconds.
    float fadeDuration = 0.15f;

    /// Height of the area the mouse points at: the caption, then the handle at its largest.
    [[nodiscard]] float getHeight() const { return captionSize + captionGap + (2.0f * handleDragRadius); }
    /// Width of that area: the rail, and the handle that sticks out of it at each end.
    [[nodiscard]] float getAreaWidth() const { return width + (2.0f * handleDragRadius); }
    /// Height of the rail, from the middle of the area.
    [[nodiscard]] float getRailY(float centreY) const
    {
        return centreY - (getHeight() / 2.0f) + captionSize + captionGap + handleDragRadius;
    }
};

/// Look of the switches (InterfaceBuilder::toggle()): its caption on the left, then on the right "OFF", a
/// short link with a dot on it, and "ON". The dot slides to the side that is on, and the label of that side
/// lights up. Sizes are in game units.
struct SwitchStyle {
    render::FontId font;
    float captionSize = 24.0f;
    float letterSpacing = 3.0f;
    float width = 560.0f;
    float height = 48.0f;
    float labelSize = 22.0f;
    float labelSpacing = 2.7f;
    /// Space between a label and the link.
    float labelGap = 14.0f;
    float linkWidth = 80.0f;
    float linkThickness = 2.0f;
    float dotRadius = 7.0f;
    /// How far the area that lights up goes beyond the labels and the link: on each side, and above and
    /// below. It is no larger than what is drawn, so the switch does not light up when the pointer is only
    /// near it.
    float hoverMargin = 8.0f;
    float hoverMarginY = 4.0f;
    /// The label of the side that is on, at rest and lit; the label of the other side.
    render::Color label{201, 205, 210, 255};
    render::Color litLabel{255, 255, 255, 255};
    render::Color dimLabel{74, 79, 85, 255};
    render::Color labelGlow{255, 255, 255, 80};
    float labelGlowBlur = 12.0f;
    render::Color link{72, 77, 83, 255};
    render::Color litLink{255, 255, 255, 190};
    render::Color dot{138, 144, 151, 255};
    render::Color litDot{255, 255, 255, 255};
    render::Color dotGlow{255, 255, 255, 90};
    float dotGlowSpread = 12.0f;
    render::Color caption{138, 144, 151, 255};
    render::Color litCaption{255, 255, 255, 255};
    /// Time for the dot to slide, and the looks to change, in seconds.
    float slideDuration = 0.18f;
};

/// Look of the gauges (InterfaceBuilder::gauge()): a fine rail, lit from its start, from transparent to
/// white, up to its value, where a spark shines. Sizes are in game units.
struct GaugeStyle {
    render::FontId font;
    float captionSize = 24.0f;
    float letterSpacing = 3.0f;
    float width = 560.0f;
    /// Space between the caption and the rail.
    float captionGap = 14.0f;
    float railThickness = 2.0f;
    float lightThickness = 3.0f;
    /// The light of a full gauge is thicker and brighter.
    float fullLightThickness = 4.0f;
    render::Color rail{38, 41, 45, 255};
    render::Color light{255, 255, 255, 255};
    render::Color glow{255, 255, 255, 60};
    /// The light and its glow of a gauge at or below its Gauge::low value.
    render::Color lowLight{239, 83, 80, 255};
    render::Color lowGlow{239, 83, 80, 90};
    float glowSpread = 6.0f;
    /// The spark at the end of the light: its half length, and its half height. Zero for none.
    float sparkLength = 26.0f;
    float sparkHeight = 9.0f;
    render::Color caption{138, 144, 151, 255};
    render::Color litCaption{255, 255, 255, 255};

    /// Height of the gauge: the caption, then the rail with room for the spark.
    [[nodiscard]] float getHeight() const { return captionSize + captionGap + (2.0f * sparkHeight); }
    /// Height of the rail, from the middle of the gauge.
    [[nodiscard]] float getRailY(float centreY) const
    {
        return centreY - (getHeight() / 2.0f) + captionSize + captionGap + sparkHeight;
    }
};

/// Look of the fields of a form (TextFieldType::Line and Code): a caption in small capitals, the value in the
/// font of the buttons, a fine rule under it which becomes a line of light with the focus, and the error in
/// red under the rule. Sizes are in game units.
struct FieldStyle {
    /// Font of the caption and of the error.
    render::FontId captionFont;
    /// Font of the value and of the digits of a code.
    render::FontId valueFont;
    float captionSize = 20.0f;
    float captionSpacing = 4.0f;
    float valueSize = 30.0f;
    float valueSpacing = 3.6f;
    float width = 600.0f;
    /// Space between the caption and the value, the value and the rule, the rule and the error.
    float captionGap = 12.0f;
    float ruleGap = 16.0f;
    float errorGap = 10.0f;
    float errorSize = 21.0f;
    float ruleThickness = 1.5f;
    /// The line of light of a focused field starts this far before it and ends this far after it.
    float lightOverhang = 80.0f;
    float lightThickness = 2.0f;
    float cursorWidth = 3.0f;
    render::Color caption{174, 179, 184, 255};
    render::Color litCaption{255, 255, 255, 255};
    render::Color value{255, 255, 255, 255};
    render::Color rule{94, 99, 105, 255};
    /// The rule of a cell of a code already typed.
    render::Color filledRule{195, 200, 205, 255};
    render::Color light{255, 255, 255, 217};
    render::Color glow{255, 255, 255, 60};
    render::Color error{232, 114, 110, 255};
    /// The cells of a code: their width, the space between them, and the size of the digits.
    float cellWidth = 88.0f;
    float cellGap = 24.0f;
    float digitSize = 60.0f;
    /// Time for a field to light up or go out, in seconds.
    float fadeDuration = 0.18f;

    /// Height of a Line field, with the room of its error.
    [[nodiscard]] float getHeight() const
    {
        return captionSize + captionGap + valueSize + ruleGap + errorGap + errorSize;
    }
    /// Height of a Code field, with the room of its error.
    [[nodiscard]] float getCodeHeight() const
    {
        return captionSize + captionGap + digitSize + ruleGap + errorGap + errorSize;
    }
    /// Width of a Code field of this many cells.
    [[nodiscard]] float getCodeWidth(std::size_t cells) const
    {
        const auto count = static_cast<float>(cells);
        return (count * cellWidth) + (std::max(count - 1.0f, 0.0f) * cellGap);
    }
};

/// Look of the rows of a list (ListRow): the title in the font of the buttons, its detail under it, a text on
/// the right, and a rule under the row, lit as the line of a button while the row is lit. Sizes are in game
/// units.
struct ListStyle {
    render::FontId titleFont;
    render::FontId detailFont;
    float height = 92.0f;
    float titleSize = 26.0f;
    float titleSpacing = 4.8f;
    float detailSize = 21.0f;
    float sideSize = 20.0f;
    float sideSpacing = 3.6f;
    /// Space between the title and its detail.
    float lineGap = 8.0f;
    float avatarSize = 56.0f;
    float avatarGap = 26.0f;
    float ruleThickness = 1.5f;
    /// The line of light of a lit row starts this far before it.
    float lightBefore = 120.0f;
    float starSize = 9.0f;
    render::Color title{230, 232, 234, 255};
    render::Color litTitle{255, 255, 255, 255};
    render::Color detail{154, 160, 166, 255};
    render::Color litDetail{195, 200, 205, 255};
    render::Color side{174, 179, 184, 255};
    render::Color rule{51, 55, 59, 255};
    render::Color disabled{94, 99, 105, 255};
};

/// Look of the avatars (Avatar): a dark disc, the picture or the initial on it, and a ring. Sizes are parts
/// of the size of the avatar, colours as the rest of the interface.
struct AvatarStyle {
    /// Font of the initial.
    render::FontId font;
    /// Size of the initial, and space between the picture and the ring.
    float initialSize = 0.38f;
    float ringGap = 0.07f;
    float ringThickness = 1.5f;
    /// Number of dots of the ring of a guest.
    int guestDots = 24;
    render::Color disc{14, 15, 17, 255};
    render::Color initial{255, 255, 255, 255};
    render::Color guestInitial{154, 160, 166, 255};
    render::Color ring{255, 255, 255, 150};
    render::Color discordRing{255, 255, 255, 230};
    render::Color glow{255, 255, 255, 70};
    float glowSpread = 0.14f;
    render::Color guestRing{107, 113, 119, 255};
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
    FlareStyle flare{};
    ConsoleStyle console{};
    ToastStyle toast{};
    TitleStyle title{};
    SliderStyle slider{};
    SwitchStyle toggle{};
    GaugeStyle gauge{};
    FieldStyle field{};
    ListStyle list{};
    AvatarStyle avatar{};
};

} // namespace rtype::engine
