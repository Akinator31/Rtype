#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>

namespace rtype::client
{

/// Name of the main menu scene.
inline constexpr const char *MainMenuScene = "main_menu";

/// Declares the main menu: a desert planet on the left, seen from orbit, a space station far on the right
/// with ships landing on it and leaving, and a column of buttons in the bottom left corner: Play, Online and
/// Settings, which open their submenu (see createSubmenuScenes()), and Quit. The ships need a FlightSystem.
/// The scene keeps updating when covered, so that the ships keep flying behind the submenus. It opens on
/// black, around the logo, then fades in. Its models are loaded before it opens (SceneSettings::assets).
/// @param resources, style, metrics, translations They must outlive the engine.
/// @param quit Called when the player presses the quit button.
void createMainMenuScene(engine::Engine &engine, render::ResourceManager &resources,
                         const engine::InterfaceStyle &style, const engine::Metrics &metrics,
                         localization::Localization &translations, std::function<void()> quit);

} // namespace rtype::client
