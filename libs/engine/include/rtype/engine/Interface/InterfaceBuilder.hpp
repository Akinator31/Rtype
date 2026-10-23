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
    ecs::entity_t createButton(const std::string &text, float width, std::function<void()> onPress,
                               Anchor anchor);

public:
    /// @param style It must outlive the builder.
    InterfaceBuilder(ecs::Ecs &ecs, SceneId scene, const InterfaceStyle &style);

    /// The scene the elements are created in.
    [[nodiscard]] SceneId scene() const { return _scene; }

    /// A text, with the font, size and colour of the style.
    ecs::entity_t text(const std::string &text, Anchor anchor);
    /// A text written from a function every frame: a score, a life count.
    ecs::entity_t boundText(std::function<std::string()> get, Anchor anchor);
    /// A rectangle, filled or outlined: a background, a frame, a placeholder.
    ecs::entity_t box(float width, float height, render::Color color, Anchor anchor, float outline = 0.0f);
    /// A texture or a part of it.
    /// @param source Part of the texture, in texture pixels.
    ecs::entity_t sprite(render::TextureId texture, render::Rectangle source, float width, float height,
                         Anchor anchor);
    /// A button running an action when confirmed or clicked, drawn as a glass Panel (InterfaceStyle::button).
    /// The buttons of a builder get the focus in the order they are created.
    ecs::entity_t button(const std::string &text, std::function<void()> onPress, Anchor anchor);
    /// A button made of text alone (PanelType::Flare, InterfaceStyle::flare), as wide as its text: a line of
    /// light traces itself across it while it has the focus. Its action runs a moment after it is pressed,
    /// once its burst has been seen (FlareStyle::pressDelay). It gets the focus in the same order as the
    /// other buttons.
    ecs::entity_t flareButton(const std::string &text, std::function<void()> onPress, Anchor anchor);
    /// A small square button with a leftwards arrow, to go back to the previous screen: a glass Panel as high
    /// as the other buttons. It gets the focus in the same order as them.
    ecs::entity_t backButton(std::function<void()> onPress, Anchor anchor);
    /// The top of a secondary screen, such as a submenu: a back button, then the title to its right,
    /// centred on the same line. The anchor places the back button; the title follows it, so the anchor
    /// should be on the left side of the screen.
    /// @return The back button.
    ecs::entity_t header(const std::string &title, std::function<void()> onBack, Anchor anchor);
};

} // namespace rtype::engine
