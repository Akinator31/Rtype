#pragma once

#include "Settings/SettingsTargets.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"

namespace rtype::client
{

/// Name of the Settings page, opened from the main menu.
inline constexpr const char *SettingsMenuScene = "settings_menu";

/// Declares the Settings page: the language, the volumes, the colour filter, the screen, the resolution, the
/// frame rate, the vertical synchronisation, and rows to the Controls, Voice and VR pages.
/// Each choice applies at once; they are written to the settings of the engine when the page closes, by its
/// back button or Escape (see PlayerSettings.hpp and DisplaySettings.hpp). It opens over the main menu like
/// its other submenus.
/// @param style Look of the interface. It must outlive the engine.
void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                         SettingsTargets targets);

} // namespace rtype::client
