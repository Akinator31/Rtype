#pragma once

#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <functional>
#include <string>

namespace rtype::ecs
{
class Ecs;
} // namespace rtype::ecs

namespace rtype::engine
{

/// Puts elements of the interface anywhere on the screen, in a scene already open: a HUD, a version number,
/// a button in a corner. Each element is placed with an Anchor, and drawn in the interface layer.
///
/// Like MenuBuilder, it only creates entities, which can be changed afterwards.
/// @code
/// engine.createScene("game", {}, [&](Engine &engine, SceneId scene) {
///     InterfaceBuilder hud(engine.getEcs(), scene, style);
///     hud.boundText([&] { return "Score: " + std::to_string(score); },
///     {AnchorPoint::TopLeft, 20.0f, 20.0f}); hud.button("Pause", [&engine] { engine.pushScene("pause"); },
///     {AnchorPoint::TopRight, -20.0f, 20.0f});
/// });
/// @endcode
class InterfaceBuilder
{
    ecs::Ecs &_ecs;
    const InterfaceStyle &_style;
    SceneId _scene;
    int _count = 0;

    ecs::entity_t element(Anchor anchor);
    ecs::entity_t createHeaderTitle(const std::string &text, Anchor backAnchor);

public:
    /// @param style It must outlive the builder.
    InterfaceBuilder(ecs::Ecs &ecs, SceneId scene, const InterfaceStyle &style);

    /// The scene the elements are created in.
    [[nodiscard]] SceneId scene() const { return _scene; }

    /// A text, with the font, size and colour of the style.
    ecs::entity_t text(const std::string &text, Anchor anchor);
    /// A title: capitals in chrome with a light around them (InterfaceStyle::title), at the size of a text of
    /// the style.
    ecs::entity_t title(const std::string &text, Anchor anchor);
    /// A title written from a function every frame, for a title that follows the language.
    ecs::entity_t boundTitle(std::function<std::string()> get, Anchor anchor);
    /// A text written from a function every frame: a score, a life count.
    ecs::entity_t boundText(std::function<std::string()> get, Anchor anchor);
    /// A rectangle, filled or outlined: a background, a frame, a placeholder.
    ecs::entity_t box(float width, float height, render::Color color, Anchor anchor, float outline = 0.0f);
    /// A texture or a part of it.
    /// @param source Part of the texture, in texture pixels.
    ecs::entity_t sprite(render::TextureId texture, render::Rectangle source, float width, float height,
                         Anchor anchor);
    /// A button made of text alone (InterfaceStyle::flare), as wide as its text: a line of light traces
    /// itself across it while it has the focus. Its action runs a moment after it is pressed, once its burst
    /// has been seen (FlareStyle::pressDelay). The buttons of a builder get the focus in the order they are
    /// created.
    ecs::entity_t button(const std::string &text, std::function<void()> onPress, Anchor anchor);
    /// A button like button(), but with no line of light nor star: its text, centred, fades smoothly to its
    /// lit look and glows while it has the focus. For an arrow of a carousel.
    ecs::entity_t arrowButton(const std::string &text, std::function<void()> onPress, Anchor anchor);
    /// A value between 0 and 1 the player sets by dragging the handle of a rail, or with Left and Right, the
    /// mouse wheel and the keys (InterfaceStyle::slider): its caption above the rail, its value as a
    /// percentage on the right. It gets the focus in the same order as the buttons. The anchor places the
    /// centre of the rail.
    /// @param get, set Read and write the setting, so that the slider always shows the real value.
    /// @param step How much a key or a notch of the wheel changes the value.
    ecs::entity_t slider(const std::string &caption, std::function<float()> get,
                         std::function<void(float)> set, float step, Anchor anchor);
    /// An on / off setting drawn as a switch (InterfaceStyle::toggle): its caption, then "OFF", a link with a
    /// dot that slides to the side that is on, and "ON". A click, Confirm, Left and Right set it, as for
    /// every Toggle. It gets the focus in the same order as the buttons. The anchor places the centre of its
    /// row.
    /// @param get, set Read and write the setting, so that the switch always shows the real value.
    ecs::entity_t toggle(const std::string &caption, std::function<bool()> get, std::function<void(bool)> set,
                         Anchor anchor);
    /// A value between 0 and 1 shown as a rail lit up to its value (InterfaceStyle::gauge): a life, a charge.
    /// It cannot be changed and does not get the focus.
    /// @param caption Written above the rail with the value as a percentage; none if empty.
    /// @param get Reads the value, so that the gauge always shows the real one.
    ecs::entity_t gauge(const std::string &caption, std::function<float()> get, Anchor anchor);
    /// A button with a leftwards arrow, to go back to the previous screen. It gets the focus in the same
    /// order as the other buttons.
    ecs::entity_t backButton(std::function<void()> onPress, Anchor anchor);
    /// The top of a secondary screen, such as a submenu: a back button, then the title to its right, written
    /// like the text of a button, but fully bright (FlareStyle::litText), and centred on the same line. The
    /// anchor places the back button; the title follows it, so the anchor
    /// should be on the left side of the screen.
    /// @return The back button.
    ecs::entity_t header(const std::string &title, std::function<void()> onBack, Anchor anchor);
    /// A header whose title is written from a function every frame, for a title that follows the language.
    /// @return The back button.
    ecs::entity_t boundHeader(std::function<std::string()> get, std::function<void()> onBack, Anchor anchor);
};

} // namespace rtype::engine
