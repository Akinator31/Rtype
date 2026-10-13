#pragma once

#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Name of the main menu scene.
inline constexpr const char *MainMenuScene = "main_menu";

/// Fills the main menu: a desert planet on the left, seen from orbit, behind the menu.
/// @param resources Where the models are loaded from. It must outlive the scene.
void buildMainMenu(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources);

} // namespace rtype::client
