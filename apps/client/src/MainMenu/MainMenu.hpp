#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/render/ResourceManager.hpp"

#include <functional>

namespace rtype::client
{

/// Name of the main menu scene.
inline constexpr const char *MainMenuScene = "main_menu";

/// Fills the main menu: a desert planet on the left, seen from orbit, a space station far on the right with
/// ships landing on it and leaving, and a column of buttons in the bottom left corner: Play, Online and
/// Settings, which do nothing yet, and Quit. The ships need a FlightSystem.
/// @param resources Where the models are loaded from. It must outlive the scene.
/// @param style Look of the buttons. It must outlive the scene.
/// @param quit Called when the player presses the quit button.
void buildMainMenu(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                   const engine::InterfaceStyle &style, const std::function<void()> &quit);

} // namespace rtype::client
