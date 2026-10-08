#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Scenes.hpp"
#include "rtype/engine/Metrics.hpp"
#include "rtype/engine/Style.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/Window.hpp"

#include <string>

namespace rtype::client
{

/// What the menus are built with and act on. Every member must outlive the menus: the widgets keep references
/// to them.
struct MenuTargets {
    ecs::Ecs &ecs;
    ecs::Scenes &scenes;
    const engine::Metrics &metrics;
    const engine::InterfaceStyle &style;
    render::Window &window;
    render::Renderer &renderer;
    audio::AudioPlayer &audio;
    /// Played when the effects volume changes, so that the player hears the new volume.
    audio::SoundId volumeSound;
    std::string &playerName;
};

/// Opens the pause menu over the game: resume, options, quit.
void openPauseMenu(const MenuTargets &targets);

/// Opens the options: player name, fullscreen, effects volume, colour filter.
void openOptionsMenu(const MenuTargets &targets);

} // namespace rtype::client
