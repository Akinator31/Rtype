#pragma once

#include "MainMenu/Layout.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/localization/Localization.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

/// The layout shared by the Settings and Controls pages: under the header, groups of rows in columns, over a
/// veil that darkens the side of the screen where they are, so that their text reads over the planet of the
/// main menu. In game units.
namespace settings_page
{

/// The top of the first group, under the header.
inline constexpr float top = 200.0f;
/// Space between the name of a group and its first row, and between two groups.
inline constexpr float labelHeight = 32.0f;
inline constexpr float groupGap = 40.0f;
/// Space between the two columns of the Settings page.
inline constexpr float columnGap = 72.0f;
/// The keys of the page, this far above the bottom of the screen.
inline constexpr float keysBottom = 64.0f;

/// The darkness of the veil on the left of the screen, and on its right.
inline constexpr render::Color veilLeft{5, 5, 8, 205};
inline constexpr render::Color veilRight{5, 5, 8, 80};
/// The veil is wider than the game, so that it still covers a wider screen.
inline constexpr float veilWidth = 2400.0f;
inline constexpr float veilHeight = 1600.0f;
/// The drawing order of the veil in the interface layer: under the rest of the page, whose elements are drawn
/// at order 1 (see InterfaceBuilder).
inline constexpr int veilOrder = 0;

/// The veil under the page. Its drawing order puts it under the rest: being created first is not enough,
/// since the ids of destroyed entities are given again in another order, and entities of the same order are
/// drawn by id.
inline ecs::entity_t addVeil(engine::InterfaceBuilder &interface, ecs::Ecs &ecs)
{
    const ecs::entity_t veil =
        interface.box(veilWidth, veilHeight, veilLeft, {engine::AnchorPoint::Left, 0.0f, 0.0f});
    ecs.getComponent<engine::Box>(veil).right = veilRight;
    ecs.getComponent<engine::Drawable>(veil).order = veilOrder;
    return veil;
}

/// The name of a group at the top of a column `width` wide, following the language.
/// @return The top of its first row.
inline float addGroup(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                      const localization::Localization &translations, const std::string &key, float left,
                      float groupTop, float width)
{
    const float inset = style.setting.inset;
    const ecs::entity_t label = interface.sectionLabel(
        translations.text(key), {engine::AnchorPoint::TopLeft, left + inset, groupTop}, width - inset);
    ecs.addComponent<engine::BoundText>(label, [&translations, key] { return translations.text(key); });
    return groupTop + labelHeight;
}

/// A text written from a translation every frame, so that it follows the language.
inline void bindText(ecs::Ecs &ecs, ecs::entity_t entity, const localization::Localization &translations,
                     std::string key)
{
    ecs.addComponent<engine::BoundText>(
        entity, [&translations, key = std::move(key)] { return translations.text(key); });
}

/// The keys of the page, at its bottom left, following the language.
inline ecs::entity_t addKeys(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                             const localization::Localization &translations, const std::string &key)
{
    const ecs::entity_t keys =
        interface.note(translations.text(key), {engine::AnchorPoint::BottomLeft, screenMargin, -keysBottom});
    bindText(ecs, keys, translations, key);
    return keys;
}

/// The widgets of a page with the key of the hint shown while they have the focus.
using Hints = std::vector<std::pair<ecs::entity_t, std::string>>;

/// A note that shows the hint of the widget that has the focus, or nothing.
inline ecs::entity_t addHint(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                             const localization::Localization &translations,
                             std::shared_ptr<const Hints> hints, engine::Anchor anchor)
{
    const ecs::entity_t hint = interface.note("", anchor);
    ecs.addComponent<engine::BoundText>(hint, [&ecs, &translations, hints = std::move(hints)] {
        for (const auto &[widget, key] : *hints) {
            if (ecs.hasComponent<engine::Focused>(widget)) {
                return translations.text(key);
            }
        }
        return std::string();
    });
    return hint;
}

} // namespace settings_page

} // namespace rtype::client
