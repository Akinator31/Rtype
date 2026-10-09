#include "Menus.hpp"

#include "rtype/engine/Interface/MenuBuilder.hpp"
#include "rtype/render/ColorFilter.hpp"

#include <string>
#include <vector>

namespace rtype::client
{

// The lambdas capture a copy of the targets, which only holds references: a menu can be opened from a widget
// of another menu without keeping anything alive itself

void openPauseMenu(const MenuTargets &targets)
{
    // An overlay: the game stays visible behind, dimmed, and keeps running for the other players
    engine::MenuBuilder pause(targets.ecs, targets.scenes, targets.metrics, targets.style, "Pause",
                              {.overlay = true});
    const engine::SceneId scene = pause.scene();
    pause.button("Resume", [targets, scene] { targets.scenes.close(scene); });
    pause.button("Options", [targets] { openOptionsMenu(targets); });
    pause.button("Quit", [targets] { targets.window.requestClose(); });
}

void openOptionsMenu(const MenuTargets &targets)
{
    // Not an overlay: the options replace the pause menu on screen instead of being drawn over it
    engine::MenuBuilder options(targets.ecs, targets.scenes, targets.metrics, targets.style, "Options");
    const engine::SceneId scene = options.scene();

    options.textField(
        "Name", targets.playerName, [targets](const std::string &text) { targets.playerName = text; }, 16);

    options.toggle(
        "Fullscreen", [targets] { return targets.window.isFullscreen(); },
        [targets](bool on) { targets.window.setFullscreen(on); });

    options.slider(
        "Effects volume", [targets] { return targets.audio.effectsVolume(); },
        [targets](float volume) {
            targets.audio.setEffectsVolume(volume);
            targets.audio.play(targets.volumeSound);
        });

    std::vector<std::string> filters;
    for (int filter = 0; filter <= static_cast<int>(render::ColorFilter::HighContrast); ++filter) {
        filters.emplace_back(render::colorFilterName(static_cast<render::ColorFilter>(filter)));
    }
    options.choice(
        "Colour filter", std::move(filters),
        [targets] { return static_cast<std::size_t>(targets.renderer.colorFilter()); },
        [targets](std::size_t filter) {
            targets.renderer.setColorFilter(static_cast<render::ColorFilter>(filter));
        });

    options.button("Back", [targets, scene] { targets.scenes.close(scene); });
}

} // namespace rtype::client
