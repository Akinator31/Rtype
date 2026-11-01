#pragma once

#include "MainMenu/Capitals.hpp"
#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <functional>
#include <optional>
#include <string>

namespace rtype::client
{

/// Where the parts of the Online screens go, in game units (docs/plan/accounts-ui.md, the frames of the
/// Série 14 of the design, twice their size).
namespace online_layout
{
/// The column of the forms, on the left, and its rows: the top of the first field, and the space between the
/// tops of two fields.
inline constexpr float columnLeft = 84.0f;
inline constexpr float fieldsTop = 250.0f;
inline constexpr float fieldsSpacing = 148.0f;
/// The space between the tops of two buttons of a column.
inline constexpr float buttonsSpacing = 76.0f;
/// The right half: Discord beside a form, the servers met, the details of an account.
inline constexpr float rightLeft = 1040.0f;
inline constexpr float rightWidth = 796.0f;
/// The line with "OR" between the two halves.
inline constexpr float dividerX = 940.0f;
} // namespace online_layout

/// The look of the Online screens: a header with its back arrow, the forms on the left, the keys at the
/// bottom left, and the small texts in the font of the captions. Each screen is built with one in its scene.
class OnlineScreen
{
    ecs::Ecs &_ecs;
    const engine::InterfaceStyle &_style;
    engine::InterfaceBuilder _ui;

public:
    /// The kinds of small texts.
    enum class Tone {
        /// White: what the player reads first.
        Bright,
        /// Silver: a detail.
        Detail,
        /// Grey: a hint, the keys.
        Hint,
        /// Red: what went wrong, or what deletes.
        Alert,
    };

    OnlineScreen(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style);

    [[nodiscard]] engine::InterfaceBuilder &ui() { return _ui; }

    /// The top of the screen: the back arrow, which runs `onBack` as Escape, and the title in capitals.
    void header(const std::string &title, std::function<void()> onBack);
    /// A dark veil over what is behind, for a question asked over another screen.
    void veil();
    /// A small text, its top left corner at the anchor.
    ecs::entity_t note(const std::string &text, engine::Anchor anchor, Tone tone = Tone::Detail,
                       float size = 22.0f);
    /// The keys of the screen, at the bottom left: "ESCAPE · BACK".
    void keys(const std::string &text);
    /// A field of the column of the forms, at its row.
    ecs::entity_t field(const std::string &caption, const std::string &content, int row,
                        std::size_t maxLength, std::function<void(const std::string &)> onSubmit = {});
    ecs::entity_t password(const std::string &caption, int row, std::size_t maxLength,
                           std::function<void(const std::string &)> onSubmit = {});
    /// A button whose text starts at the anchor.
    ecs::entity_t button(const std::string &text, std::function<void()> onPress, engine::Anchor anchor);
    ecs::entity_t dangerButton(const std::string &text, std::function<void()> onPress, engine::Anchor anchor);
    /// A button with the logo of Discord before it, at the anchor.
    ecs::entity_t discordButton(const std::string &text, std::function<void()> onPress, engine::Anchor anchor,
                                render::TextureId logo);
    /// The line between the form and Discord, from `top` to `bottom`, with "OR" in its middle.
    void divider(float top, float bottom, const std::string &text);
    /// A rule across, `width` long, its left end at the anchor.
    void rule(engine::Anchor anchor, float width, render::Color color);
    /// The title of a question asked over a veil, in chrome, centred, its top at `top`.
    ecs::entity_t question(const std::string &text, float top);
};

/// The first character of a name, in capitals, for an avatar without picture: a whole character of UTF-8.
[[nodiscard]] std::string initialOf(const std::string &name);

} // namespace rtype::client
