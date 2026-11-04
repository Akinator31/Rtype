#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// Name of the Settings page, opened from the main menu.
inline constexpr const char *SettingsMenuScene = "settings_menu";

/// What the Settings page changes. Everything must outlive the engine.
struct SettingsTargets {
    localization::Localization &translations;
    audio::AudioPlayer &audio;
    render::Renderer &renderer;
    render::Window &window;
};

/// Declares the Settings page: the language, the volumes, the colour filter, the screen and the resolution.
/// Each choice applies at once; they are written to the settings of the engine when the page closes, by its
/// back button or Escape (see PlayerSettings.hpp and DisplaySettings.hpp). It opens over the main menu like
/// its other submenus.
/// @param style Look of the interface. It must outlive the engine.
void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                         SettingsTargets targets);

} // namespace rtype::client
