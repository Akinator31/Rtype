#pragma once

#include "Console/Console.hpp"
#include "Game/GameScenes.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"

namespace rtype::client
{

/// Declares the scenes shown while the assets load: the loading screen, and the console, which opens over
/// any scene. Everything given must outlive the engine.
void createLoadingScenes(engine::Engine &engine, render::ResourceManager &resources,
                         const engine::InterfaceStyle &style, Console &console,
                         const engine::RendererMetrics &metrics, const localization::Localization &translations);

/// Declares the scenes of the client built from the loaded assets: the main menu, its submenus and the
/// hangar. Those of a game are declared by GameScenes, those of online play by OnlineScenes. Everything given
/// must outlive the engine.
void createMenuScenes(engine::Engine &engine, render::ResourceManager &resources,
                      const engine::InterfaceStyle &style, render::Window &window,
                      const engine::RendererMetrics &metrics, GameScenes &games, const game::GameConfig &config,
                      localization::Localization &translations, ShipSelection &ships);

} // namespace rtype::client
