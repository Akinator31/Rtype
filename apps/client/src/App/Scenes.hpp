#pragma once

#include "Console/Console.hpp"
#include "Game/GameScenes.hpp"
#include "Settings/SettingsTargets.hpp"
#include "Settings/VrSettings.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"

namespace rtype::client
{

/// Declares the scenes shown while the assets load: the loading screen, and the console, which opens over
/// any scene. Everything given must outlive the engine.
void createLoadingScenes(engine::Engine &engine, render::ResourceManager &resources,
                         const engine::InterfaceStyle &style, Console &console,
                         const engine::RendererMetrics &metrics,
                         const localization::Localization &translations);

/// Declares the scenes of the client built from the loaded assets: the main menu, its submenus, the hangar
/// and the story. Those of a game are declared by GameScenes, those of online play by OnlineScenes.
/// Everything given must outlive the engine.
/// @param targets What the Settings page changes; its translations are those of every menu.
/// @param vr What the VR page changes.
void createMenuScenes(engine::Engine &engine, render::ResourceManager &resources,
                      const engine::InterfaceStyle &style, const engine::RendererMetrics &metrics,
                      GameScenes &games, const game::GameConfig &config, const SettingsTargets &targets,
                      const VrTargets &vr, ShipSelection &ships);

} // namespace rtype::client
