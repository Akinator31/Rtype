#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"

namespace rtype::client
{

/// Names of the submenus of the main menu, opened by its buttons.
inline constexpr const char *PlayMenuScene = "play_menu";
inline constexpr const char *OnlineMenuScene = "online_menu";
inline constexpr const char *SettingsMenuScene = "settings_menu";

/// Declares the submenus of the main menu. Each one opens over the main menu, whose world stays visible but
/// not its buttons, and shows its title after a back button that closes it, like Escape. Their content is to
/// come.
/// @param style Look of the interface. It must outlive the engine.
void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style);

} // namespace rtype::client
