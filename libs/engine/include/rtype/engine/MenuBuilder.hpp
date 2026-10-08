#pragma once

#include "rtype/ecs/Entity/Entity.hpp"
#include "rtype/ecs/Scenes.hpp"
#include "rtype/engine/Metrics.hpp"
#include "rtype/engine/Style.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace rtype::ecs
{
class Ecs;
} // namespace rtype::ecs

namespace rtype::engine
{

/// Builds a menu in its own scene: a background, a title, and widgets stacked in a column at the centre.
///
/// It only creates entities: everything it builds can be changed afterwards like any other entity, for
/// instance to add an animation or an icon.
/// @code
/// MenuBuilder pause(ecs, scenes, metrics, style, "Pause", {.overlay = true});
/// pause.button("Resume", [&scenes] { scenes.close(); });
/// pause.toggle("Fullscreen", [&] { return window.isFullscreen(); }, [&](bool on) { window.setFullscreen(on);
/// });
/// @endcode
class MenuBuilder
{
    ecs::Ecs &_ecs;
    const InterfaceStyle &_style;
    ecs::SceneId _scene;
    ecs::entity_t _column;
    int _count = 0;

    ecs::entity_t widget(const std::string &text, bool focusable);

public:
    /// Opens the scene of the menu and creates its background and title.
    /// @param settings Settings of the scene: an overlay keeps the scene below visible, dimmed.
    MenuBuilder(ecs::Ecs &ecs, ecs::Scenes &scenes, const Metrics &metrics, const InterfaceStyle &style,
                const std::string &title, ecs::SceneSettings settings = {});

    /// The scene of the menu.
    [[nodiscard]] ecs::SceneId scene() const { return _scene; }

    /// A text that cannot get the focus.
    ecs::entity_t label(const std::string &text);
    /// A button running an action when confirmed or clicked.
    ecs::entity_t button(const std::string &text, std::function<void()> onPress);
    /// An on / off setting.
    ecs::entity_t toggle(const std::string &caption, std::function<bool()> get,
                         std::function<void(bool)> set);
    /// A value between 0 and 1, changed by `step`.
    ecs::entity_t slider(const std::string &caption, std::function<float()> get,
                         std::function<void(float)> set, float step = 0.1f);
    /// One option among several.
    ecs::entity_t choice(const std::string &caption, std::vector<std::string> options,
                         std::function<std::size_t()> get, std::function<void(std::size_t)> set);
    /// A text field. Its TextInput is reachable through the TextField component, to set a filter for
    /// instance.
    ecs::entity_t textField(const std::string &caption, const std::string &content,
                            std::function<void(const std::string &)> onSubmit = {},
                            std::size_t maxLength = 0);
};

} // namespace rtype::engine
