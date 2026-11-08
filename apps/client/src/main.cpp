#include "App/Configuration.hpp"
#include "App/Scenes.hpp"
#include "App/Systems.hpp"
#include "Assets.hpp"
#include "Commands/Commands.hpp"
#include "Commands/FpsCommand.hpp"
#include "Console/Console.hpp"
#include "Game/GameScenes.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Loading/AssetPreload.hpp"
#include "Loading/LoadingScene.hpp"
#include "Loading/LoadingSystem.hpp"
#include "Loading/SceneAssetLoader.hpp"
#include "Loading/SceneLoadStatus.hpp"
#include "MainMenu/MainMenu.hpp"
#include "Online/OnlineScenes.hpp"
#include "Quests/Quests.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/audio/VoiceStreams.hpp"
#include "rtype/discord/SocialSdkAuthorizer.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"
#include "rtype/voice/Microphone.hpp"

#include <cstdlib>
#include <exception>
#include <string_view>

namespace
{

constexpr float CollisionCellSize = 64.0f;
/// How loud the buttons are: the hover is heard at each move in a menu, the click file is quiet.
constexpr float ButtonHoverVolume = 0.4f;
constexpr float ButtonClickVolume = 1.0f;

struct StopWorkers {
    rtype::engine::ThreadManager &threads;

    ~StopWorkers() { threads.stopAll(); }
};

/// Ends the game played before the devices of the voice its session uses.
struct StopSessions {
    rtype::client::GameSessionSystem &sessions;

    ~StopSessions() { sessions.stop(); }
};

/// Unloads the assets of the scenes before the ResourceManager and the AudioPlayer that hold them.
struct UnloadSceneAssets {
    rtype::engine::Engine &engine;

    ~UnloadSceneAssets() { engine.unloadSceneAssets(); }
};

void run()
{
    using namespace rtype::render;

    Window window({.fullscreen = true});
    rtype::engine::Engine engine;
    rtype::client::readSettings(engine.getSettings());
    const rtype::audio::AudioDevice audioDevice;
    // The voice of the games online: the microphone, opened only while it is on in a game, and the voices of
    // the others
    rtype::voice::Microphone microphone;
    rtype::audio::VoiceStreams voiceStreams(audioDevice);
    rtype::client::VoiceDevices voice{.capture = microphone, .playback = voiceStreams};
    rtype::audio::AudioPlayer audio(audioDevice, applicationDirectory() / "assets",
                                    engine.getThreads().getAssetsWorker());
    ResourceManager resources(window, applicationDirectory() / "assets",
                              engine.getThreads().getAssetsWorker());
    StopWorkers stopWorkers{engine.getThreads()};
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);

    const FontId display = resources.loadFont(RTYPE_FONT_DISPLAY, 96);
    const FontId messageFont = resources.loadFont(RTYPE_FONT_BUTTON, 96);
    const rtype::engine::InterfaceStyle style{
        .font = resources.loadFont(RTYPE_FONT_TITLE, 160),
        .button = {.font = resources.loadFont(RTYPE_FONT_BUTTON, 96),
                   .hoverSound = rtype::engine::SoundEffect{audio.loadSound(RTYPE_SOUND_BUTTON_HOVER),
                                                            {.volume = ButtonHoverVolume}},
                   .clickSound = rtype::engine::SoundEffect{audio.loadSound(RTYPE_SOUND_BUTTON_CLICK),
                                                            {.volume = ButtonClickVolume}}},
        .flare = {.font = resources.loadFont(RTYPE_FONT_FLARE, 96)},
        .toast = {.nameFont = display, .messageFont = messageFont},
        .title = {.font = display},
        .setting = {.captionFont = messageFont, .valueFont = display},
        .slider = {.font = messageFont, .valueFont = display},
        .keys = {.font = display},
        .toggle = {.font = display},
        .gauge = {.font = display},
        .field = {.captionFont = messageFont, .valueFont = display},
        .list = {.titleFont = display, .detailFont = messageFont},
        .avatar = {.font = display},
    };
    const rtype::engine::RendererMetrics metrics(renderer);
    rtype::localization::Localization translations("en");
    const auto localeDirectory = applicationDirectory() / "assets" / "locales";
    translations.load("en", localeDirectory / "en.lang");
    translations.load("fr", localeDirectory / "fr.lang");
    const rtype::client::SettingsTargets settingsTargets{translations, audio, renderer, window};
    // The choices of the player at the last run
    rtype::client::applySavedSettings(engine, settingsTargets);

    rtype::client::Console console;
    const rtype::game::GameConfig config = rtype::client::readGameConfig(console);
    // The network of the client, polled by the game loop: it outlives everything that uses it
    rtype::network::EventLoop network;
    // Signing in with Discord, polled by the game loop too
    rtype::discord::SocialSdkAuthorizer discord;
    engine.setInputSource(devices);
    rtype::client::GameSessionSystem &sessions =
        rtype::client::registerClientSystems(engine, devices, audio, metrics, style);
    StopSessions stopSessions{sessions};
    rtype::client::SceneAssetLoader sceneAssets(resources, audio);
    UnloadSceneAssets unloadSceneAssets{engine};
    rtype::client::AssetPreload preload(resources);
    // Last: it only writes opacities, read when drawing
    const auto &loading = engine.getEcs().registerSystem<rtype::client::LoadingSystem>(preload);
    rtype::engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);
    rtype::engine::ColliderDebugRenderer colliderDebugRenderer(engine.getEcs(), engine.getScenes(),
                                                               CollisionCellSize);
    const char *debugEnvironment = std::getenv("RTYPE_DEBUG_COLLIDERS");
    rtype::client::DebugViews debugViews{.hitboxes =
                                             debugEnvironment && std::string_view(debugEnvironment) == "1"};
    bool debugKeyWasDown = false;
    rtype::client::SceneLoadStatus sceneLoadStatus(engine, devices);
    bool assetsLoaded = false;

    const auto playFrame = [&] {
        const bool debugKeyDown = devices.isKeyDown(rtype::input::Key::F3);
        if (debugKeyDown && !debugKeyWasDown) {
            debugViews.hitboxes = !debugViews.hitboxes;
        }
        debugKeyWasDown = debugKeyDown;
        sceneLoadStatus.update();
        window.update();
        network.poll();
        discord.poll();
        engine.update(window.frameTime());
        if (assetsLoaded) {
            resources.finishPendingLoads();
        }
        audio.finishPendingLoads();
        audio.update();
        renderer.beginFrame();
        sceneRenderer.draw(renderer, rtype::engine::Layer::World);
        if (debugViews.hitboxes) {
            colliderDebugRenderer.draw(renderer);
        }
        sceneRenderer.draw(renderer, rtype::engine::Layer::Interface);
        if (debugViews.fps) {
            rtype::client::drawFpsCounter(renderer);
        }
        sceneLoadStatus.draw(renderer);
        renderer.endFrame();
    };

    rtype::client::createLoadingScenes(engine, resources, style, console, metrics, translations);
    engine.setSceneAssetLoader(sceneAssets, rtype::client::LoadingScene);
    engine.switchScene(rtype::client::LoadingScene);
    while (!window.shouldClose() && !loading.isFinished()) {
        playFrame();
    }
    if (window.shouldClose()) {
        return;
    }

    assetsLoaded = true;
    // The scenes of the game are built from the loaded assets, which they find in the cache
    // The ship equipped at the last run
    rtype::client::ShipSelection ships;
    rtype::client::readEquippedShip(ships, engine.getSettings());
    rtype::client::GameScenes games(engine, resources, audio, metrics, style, sessions, translations, ships);
    rtype::client::OnlineScenes online(engine, resources, network, games, style, translations, discord, ships,
                                       rtype::render::userDataDirectory() / "pictures", config, voice);
    rtype::client::createMenuScenes(engine, resources, style, metrics, games, config, settingsTargets, ships);
    rtype::client::registerClientCommands(console, engine, window, games, config, debugViews, voice);
    rtype::client::registerQuests(engine, translations);
    engine.switchScene(rtype::client::MainMenuScene);
    while (!window.shouldClose()) {
        playFrame();
    }
}

} // namespace

int main()
{
    const rtype::log::Logger logger("client");

    if (const auto problem = rtype::log::configureFromEnvironment()) {
        logger.warning("{}", *problem);
    }
    try {
        run();
    } catch (const std::exception &error) {
        logger.error("{}", error.what());
        return 84;
    }
    return 0;
}
