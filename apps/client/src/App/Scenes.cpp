#include "App/Scenes.hpp"

#include "Campaign/CampaignScene.hpp"
#include "Console/ConsoleScene.hpp"
#include "Game/LocalSession.hpp"
#include "Loading/LoadingScene.hpp"
#include "Lore/LoreScene.hpp"
#include "MainMenu/ControlsMenu.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/SettingsMenu.hpp"
#include "MainMenu/Submenus.hpp"
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
                      ShipSelection &ships, Campaign &campaign)
{
    localization::Localization &translations = targets.translations;
    createMainMenuScene(engine, resources, style, metrics, translations,
                        [&window = targets.window] { window.requestClose(); });
    const auto playOffline = [&games, &config] {
        games.play(std::make_unique<LocalSession>(game::Rules{}, config));
    };
    const auto playSolo = [&engine, &ships, playOffline] { openShipSelector(engine, ships, playOffline); };
    createSubmenuScenes(engine, style, translations, playSolo,
                        [&engine, &ships] { openShipSelector(engine, ships); });
    createSettingsScene(engine, style, targets);
    createControlsScenes(engine, style, translations);
    createShipSelectorScene(engine, resources, style, translations, ships);
    createLoreScene(engine, resources, targets.audio, style, metrics, translations);
    createCampaignEndingScene(engine, resources, targets.audio, style, metrics, translations);
    // After its briefing, each mission flies the ship equipped straight into space, without the hangar
    createCampaignScene(engine, resources, targets.audio, metrics, translations, campaign);
}

} // namespace rtype::client
