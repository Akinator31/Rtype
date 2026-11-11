#include "App/Configuration.hpp"
#include "App/Scenes.hpp"
#include "App/Systems.hpp"
#include "Assets.hpp"
#include "Commands/Commands.hpp"
#include "Commands/FpsCommand.hpp"
#include "Console/Console.hpp"
#include "Discord/DiscordPresence.hpp"
#include "Game/EntityLook.hpp"
#include "Game/EntitySounds.hpp"
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
#include "Settings/VrSettings.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "Voice/VoiceKeysSystem.hpp"
#include "Vr/CockpitLife.hpp"
#include "Vr/VrControllers.hpp"
#include "Vr/VrHaptics.hpp"
#include "Vr/VrMode.hpp"
#include "Vr/VrView.hpp"
#include "rtype/audio/AudioDevice.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/audio/OutputLoopback.hpp"
#include "rtype/audio/VoiceStreams.hpp"
#include "rtype/discord/Session.hpp"
#include "rtype/discord/SocialSdkAuthorizer.hpp"
#include "rtype/discord/SocialSdkConnection.hpp"
#include "rtype/discord/SocialSdkPresence.hpp"
#include "rtype/discord/SocialSdkVoiceCall.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/input/VirtualInput.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"
#include "rtype/voice/Microphone.hpp"
#include "rtype/xr/Headset.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string_view>
#include <vector>

namespace
{

constexpr float CollisionCellSize = 64.0f;
/// How loud the buttons are: the hover is heard at each move in a menu, the click file is quiet.
constexpr float ButtonHoverVolume = 0.4f;
constexpr float ButtonClickVolume = 1.0f;
/// How loud the alarm of the cockpit is in a VR headset: heard over the battle, not over the voices.
constexpr float CockpitAlarmVolume = 0.55f;
/// How large the line of the window is while it shows nothing of the headset (VrMirror::Nothing).
constexpr float InHeadsetTextSize = 48.0f;

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

void run(const std::vector<std::string_view> &arguments)
{
    using namespace rtype::render;

    Window window({.fullscreen = true});
    rtype::engine::Engine engine;
    rtype::client::readSettings(engine.getSettings());
    const rtype::audio::AudioDevice audioDevice;
    // The voice of the games online: the microphone, opened only while the player can talk in a game or tries
    // it on the Voice page, the voices of the others, and what the speakers play, for the echo cancellation
    rtype::voice::Microphone microphone;
    rtype::audio::VoiceStreams voiceStreams(audioDevice);
    rtype::audio::OutputLoopback loopback(audioDevice);
    rtype::client::VoiceDevices voice{.capture = microphone, .playback = voiceStreams, .loopback = &loopback};
    rtype::audio::AudioPlayer audio(audioDevice, applicationDirectory() / "assets",
                                    engine.getThreads().getAssetsWorker());
    ResourceManager resources(window, applicationDirectory() / "assets",
                              engine.getThreads().getAssetsWorker());
    StopWorkers stopWorkers{engine.getThreads()};
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);
    // What the game reads: the devices, and the controllers of a VR headset over them, as a gamepad and a
    // pointer, while it shows the game
    rtype::input::VirtualInput input(devices);

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
    // Not const: while a VR headset shows the game, the surfaces of the interface are drawn apart
    rtype::engine::RendererMetrics metrics(renderer);
    rtype::localization::Localization translations("en");
    const auto localeDirectory = applicationDirectory() / "assets" / "locales";
    translations.load("en", localeDirectory / "en.lang");
    translations.load("fr", localeDirectory / "fr.lang");
    rtype::client::AuraColors auraColors;
    // The one client of the Discord Social SDK, polled by the game loop, and the activity of the player shown
    // in its Discord profile
    rtype::discord::Session discordSession;
    rtype::discord::SocialSdkPresence discordPresence(
        discordSession, rtype::client::readDiscordApplication(engine.getSettings()));
    rtype::client::PresenceTracker presence(discordPresence, translations,
                                            rtype::client::readDiscordActivity(engine.getSettings()));
    const rtype::client::SettingsTargets settingsTargets{translations, audio, renderer,   window,
                                                         auraColors,   voice, microphone, presence};
    // The choices of the player at the last run
    rtype::client::applySavedSettings(engine, settingsTargets);

    rtype::client::Console console;
    const rtype::game::GameConfig config = rtype::client::readGameConfig(console);
    // The network of the client, polled by the game loop: it outlives everything that uses it
    rtype::network::EventLoop network;
    // Signing in with Discord, on the client of the Social SDK, and connecting it with the token of a server
    rtype::discord::SocialSdkAuthorizer discord(discordSession);
    rtype::discord::SocialSdkConnection discordConnection(discordSession);
    // The voice of the games through their Discord lobby, for a player connected to Discord
    rtype::discord::SocialSdkVoiceCall discordCall(discordSession);
    voice.discordCall = &discordCall;
    // Connected, the activity shown before is cleared by Discord: shown again
    discordConnection.onStatusChanged([&presence](rtype::discord::ConnectionStatus status) {
        if (status == rtype::discord::ConnectionStatus::Ready) {
            presence.resend();
        }
    });
    engine.setInputSource(input);
    const rtype::client::ClientSystems systems =
        rtype::client::registerClientSystems(engine, input, audio, metrics, style);
    rtype::client::GameSessionSystem &sessions = systems.sessions;
    sessions.setAuraColors(&auraColors);
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
    // The game in a VR headset, with --xr or the VR setting: opened on the OpenGL context of the window, and
    // closed before it. Without a headset, the game stays on the screen and says so once in the main menu.
    // While the headset shows the game, the world of the game is laid on the board in relief for each eye, or
    // drawn once into the image of the board while it is flat; the window shows what the VR page says, the
    // left eye by default
    rtype::client::VrPreferences vrPreferences = rtype::client::readVrSettings(engine.getSettings());
    const auto drawWorld = [&] {
        sceneRenderer.draw(renderer, rtype::engine::Layer::World);
        if (debugViews.hitboxes) {
            colliderDebugRenderer.draw(renderer);
        }
    };
    rtype::client::VrView vrView(renderer, resources, drawWorld, rtype::client::layoutOf(vrPreferences));
    // The cockpit reacts to the game: its lights, and its alarm
    rtype::client::CockpitLife cockpitLife;
    const rtype::audio::SoundId cockpitAlarm = audio.loadSound(RTYPE_SOUND_COCKPIT_ALARM);
    bool wasInHeadset = false;
    // The controllers: a gamepad, a pointer at the panel, drawn in the hands; and they vibrate with the game
    rtype::client::VrControllers controllers;
    rtype::client::VrHaptics haptics;
    rtype::client::VrMode vr(
        {.open = [] { return rtype::xr::openHeadset(); },
         .pace = [&window](bool paced) { window.setPacedElsewhere(paced); },
         .lost =
             [&engine, &translations] {
                 engine.showToast(translations.text("vr.toast.lost"), rtype::engine::ToastType::Warning);
             },
         .drawEye =
             [&vrView, &controllers](int eye, const rtype::xr::EyeView &view) {
                 vrView.drawEye(eye, view, controllers.looks());
             },
         .located =
             [&vrView, &controllers](const std::array<rtype::xr::HandState, rtype::xr::HandCount> &hands) {
                 controllers.place(hands, vrView.panelSurface());
             }});
    bool recenterKeyWasDown = false;
    const bool vrWanted = rtype::client::wantsVr(arguments, engine.getSettings());
    if (vrWanted) {
        vr.start();
    }
    // What the VR page changes
    const rtype::client::VrTargets vrTargets{.preferences = vrPreferences,
                                             .switchHeadset =
                                                 [&vr](bool open) {
                                                     if (open) {
                                                         return vr.start();
                                                     }
                                                     vr.close();
                                                     return false;
                                                 },
                                             .recenter = [&vr] { vr.recenter(); }};
    // The window while it shows nothing of the headset, and the line it says
    const auto drawInHeadset = [&renderer, &translations, display] {
        renderer.beginFrame();
        const std::string text = translations.text("vr.window.in_headset");
        const rtype::render::Rectangle area = renderer.visibleArea();
        const rtype::render::Vector2 size = renderer.measureText(display, text, InHeadsetTextSize);
        renderer.drawText(display, text,
                          {area.x + ((area.width - size.x) / 2.0f), area.y + ((area.height - size.y) / 2.0f)},
                          InHeadsetTextSize, rtype::render::colors::Gray);
        renderer.endFrame();
    };

    const auto playFrame = [&] {
        const bool debugKeyDown = devices.isKeyDown(rtype::input::Key::F3);
        if (debugKeyDown && !debugKeyWasDown) {
            debugViews.hitboxes = !debugViews.hitboxes;
        }
        debugKeyWasDown = debugKeyDown;
        const bool recenterKeyDown = devices.isKeyDown(rtype::client::RecenterKey);
        if (recenterKeyDown && !recenterKeyWasDown) {
            vr.recenter();
        }
        recenterKeyWasDown = recenterKeyDown;
        // The controllers, read at the last frame of the headset, before the engine reads the input
        controllers.update({.elapsed = window.frameTime(),
                            .showing = vr.isShowing(),
                            .panelOpen = vrView.panelSurface().has_value(),
                            .hovered = systems.interface.hovered(),
                            .gamepadUsed = rtype::client::isGamepadUsed(devices),
                            .snapTurn = vrPreferences.snapTurn});
        input.setGamepad(controllers.gamepad());
        input.setPointer(controllers.pointer());
        if (controllers.recenterAsked()) {
            vr.recenter();
        }
        if (controllers.turnAsked() != 0.0f) {
            vr.turn(controllers.turnAsked());
        }
        for (const auto &[hand, vibration] : controllers.takeVibrations()) {
            vr.vibrate(hand, vibration);
        }
        // While the headset shows the game, the anchors place each part of the interface on its surface
        metrics.separateSurfaces(
            vr.isShowing() ? std::optional(rtype::client::surfaceAreas(renderer.gameSize())) : std::nullopt);
        sceneLoadStatus.update();
        window.update();
        network.poll();
        discordSession.poll();
        engine.update(window.frameTime());
        if (assetsLoaded) {
            resources.finishPendingLoads();
        }
        audio.finishPendingLoads();
        audio.update();
        const bool inHeadset = vr.isShowing();
        const bool playing = inHeadset && sessions.running();
        for (const auto &[hand, vibration] : haptics.update(
                 {.playing = playing, .shots = sessions.localShots(), .health = sessions.localHealth()})) {
            vr.vibrate(hand, vibration);
        }
        // The voices of the other players, from the side of the board their ship is on, in the headset only
        std::array<float, rtype::voice::MaxParticipants> voicePans{};
        if (playing) {
            for (const auto &[slot, x] : sessions.ships()) {
                if (slot < voicePans.size()) {
                    voicePans.at(slot) = rtype::client::soundPan(x);
                }
            }
        }
        for (rtype::voice::Slot slot = 0; slot < voicePans.size(); ++slot) {
            voiceStreams.setPan(slot, voicePans.at(slot));
        }
        if (inHeadset) {
            if (!wasInHeadset) {
                vrView.arrive();
            }
            const rtype::client::GameSession *session = sessions.session();
            cockpitLife.update(window.frameTime(),
                               {.playing = playing,
                                .health = sessions.localHealth(),
                                .maxHealth = sessions.localMaxHealth(),
                                .boss = sessions.bossIsThere(),
                                .pilot = rtype::client::playerColor(session ? session->localSlot() : 0)});
            if (cockpitLife.takeAlarm()) {
                audio.play(cockpitAlarm, {.volume = CockpitAlarmVolume});
            }
            // The HUD on the screens of the cockpit, the menus on the panel, the world on the board
            vrView.setLayout(rtype::client::layoutOf(vrPreferences));
            vrView.update(
                window.frameTime(),
                {.light = sceneRenderer.light(),
                 .sky = sceneRenderer.sky(),
                 .playing = sessions.running(),
                 .panel = sceneRenderer.hasContent(rtype::engine::Surface::Panel),
                 .scenes = engine.getScenes().bottom(),
                 .relief = !vrPreferences.flatBoard && rtype::client::canDrawInRelief(sceneRenderer.camera()),
                 .cockpit = cockpitLife.lights()});
            sceneRenderer.setToastSurface(vrView.toastSurface());
            vrView.drawFrames([&](rtype::engine::Surface surface) {
                sceneRenderer.draw(renderer, surface);
                if (surface == rtype::engine::Surface::Board && debugViews.fps) {
                    rtype::client::drawFpsCounter(renderer);
                }
                if (surface == rtype::engine::Surface::Panel) {
                    sceneLoadStatus.draw(renderer);
                }
            });
        } else {
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
        }
        vr.update();
        wasInHeadset = inHeadset;
        if (inHeadset) {
            // The window: what the left eye sees, the game flat as on the board, or a line
            switch (vrPreferences.mirror) {
            case rtype::client::VrMirror::LeftEye:
                renderer.showMirror();
                break;
            case rtype::client::VrMirror::FlatGame:
                renderer.beginFrame();
                drawWorld();
                sceneRenderer.draw(renderer, rtype::engine::Surface::Board);
                renderer.endFrame();
                break;
            case rtype::client::VrMirror::Nothing:
                drawInHeadset();
                break;
            }
        }
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
    // The ships and the weapons the level of the last account opens, and those equipped at the last run
    rtype::client::ShipSelection ships;
    rtype::client::readRequiredLevels(ships, config.shipUnlocks);
    rtype::client::readHangarWeapons(ships, config.weaponUnlocks);
    rtype::client::readPlayerLevel(ships, engine.getSettings());
    rtype::client::readEquippedShip(ships, engine.getSettings());
    rtype::client::readEquippedWeapons(ships, engine.getSettings());
    rtype::client::GameScenes games(engine, resources, audio, metrics, style, sessions, translations, ships,
                                    config.ships, config.items, config);
    rtype::client::OnlineScenes online(engine, resources, network, games, style, translations, discord,
                                       discordConnection, ships,
                                       rtype::render::userDataDirectory() / "pictures", config, voice);
    rtype::client::createMenuScenes(engine, resources, style, metrics, games, config, settingsTargets,
                                    vrTargets, ships);
    rtype::client::registerClientCommands(console, engine, window, games, config, debugViews);
    rtype::client::registerQuests(engine, translations);
    engine.getEcs().registerSystem<rtype::client::VoiceKeysSystem>(engine, sessions, voice, translations);
    engine.switchScene(rtype::client::MainMenuScene);
    if (vrWanted && !vr.isActive()) {
        engine.showToast(translations.text("vr.toast.no_headset"), rtype::engine::ToastType::Warning);
    }
    while (!window.shouldClose()) {
        playFrame();
        presence.update(rtype::client::observeActivity(engine, sessions, ships),
                        std::chrono::steady_clock::now());
    }
}

} // namespace

int main(int argc, char **argv)
{
    const rtype::log::Logger logger("client");

    if (const auto problem = rtype::log::configureFromEnvironment()) {
        logger.warning("{}", *problem);
    }
    try {
        run(std::vector<std::string_view>(argv + 1, argv + argc));
    } catch (const std::exception &error) {
        logger.error("{}", error.what());
        return 84;
    }
    return 0;
}
