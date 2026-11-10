#include "App/Scenes.hpp"

#include "Console/ConsoleScene.hpp"
#include "Game/LocalSession.hpp"
#include "Loading/LoadingScene.hpp"
#include "Lore/LoreScene.hpp"
#include "MainMenu/ControlsMenu.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/SettingsMenu.hpp"
#include "MainMenu/Submenus.hpp"
#include "MainMenu/VoiceMenu.hpp"
#include "ShipSelector/ShipSelector.hpp"

#include <memory>

namespace rtype::client
{

void createLoadingScenes(engine::Engine &engine, render::ResourceManager &resources,
                         const engine::InterfaceStyle &style, Console &console,
                         const engine::RendererMetrics &metrics,
                         const localization::Localization &translations)
{
    engine.createScene(LoadingScene, {.onBack = [] {}},
                       [&resources, &style, &translations](engine::Engine &target, engine::SceneId scene) {
                           buildLoadingScene(target, scene, resources, style, translations);
                       });
    engine.createScene(ConsoleScene, {.overlay = true},
                       [&console, &metrics, &style](engine::Engine &target, engine::SceneId scene) {
                           buildConsole(target, scene, console, metrics, style);
                       });
}

void createMenuScenes(engine::Engine &engine, render::ResourceManager &resources,
                      const engine::InterfaceStyle &style, const engine::RendererMetrics &metrics,
                      GameScenes &games, const game::GameConfig &config, const SettingsTargets &targets,
                      ShipSelection &ships)
{
    localization::Localization &translations = targets.translations;
    createMainMenuScene(engine, resources, style, metrics, translations,
                        [&window = targets.window] { window.requestClose(); });
    createSubmenuScenes(
        engine, style, translations,
        [&engine, &ships, &games, &config] {
            openShipSelector(engine, ships, [&games, &config] {
                games.play(std::make_unique<LocalSession>(game::Rules{}, config));
            });
        },
        [&engine, &ships] { openShipSelector(engine, ships); });
    createSettingsScene(engine, style, targets);
    createControlsScenes(engine, style, translations);
    createVoiceScenes(engine, style, targets);
    createShipSelectorScene(engine, resources, style, translations, ships);
    createLoreScene(engine, resources, targets.audio, style, metrics, translations);
}

} // namespace rtype::client
