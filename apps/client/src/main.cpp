#include "Assets.hpp"
#include "Commands/Commands.hpp"
#include "Console/Console.hpp"
#include "Console/ConsoleScene.hpp"
#include "Console/ConsoleSystem.hpp"
#include "Game/GameScenes.hpp"
#include "Game/GameSessionSystem.hpp"
#include "Game/LocalSession.hpp"
#include "MainMenu/FlightSystem.hpp"
#include "MainMenu/IntroSystem.hpp"
#include "MainMenu/MainMenu.hpp"
#include "MainMenu/Submenus.hpp"
#include "Online/OnlineScenes.hpp"
#include "ThreadedInput.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/physics/Physics.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/RenderCommands.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{

constexpr float CollisionCellSize = 64.0f;

bool preloadAssets(rtype::render::Window &window, rtype::render::Renderer &renderer,
                   rtype::render::ResourceManager &resources)
{
    using namespace rtype::render;
    const auto title = resources.loadFontAsync(RTYPE_FONT_TITLE, 160);
    const auto button = resources.loadFontAsync(RTYPE_FONT_BUTTON, 96);
    const auto sky = resources.loadTextureAsync(RTYPE_IMAGE_STARFIELD, TextureFiltering::Smooth);
    constexpr std::array modelPaths{
        RTYPE_MODEL_PLANET,
        RTYPE_MODEL_PLANET_DUST,
        RTYPE_MODEL_RING_ASTEROIDS_INNER,
        RTYPE_MODEL_RING_ASTEROIDS_OUTER,
        RTYPE_MODEL_RING_DUST,
        RTYPE_MODEL_STATION,
        RTYPE_MODEL_SHIP_EZNO,
        RTYPE_MODEL_SHIP_SPACESHIP,
        RTYPE_MODEL_SHIP_BATTLESTAR,
        RTYPE_MODEL_SHIP_DROP_SHIP,
    };
    std::vector<std::shared_future<ModelId>> models;
    for (const char *path : modelPaths) {
        models.push_back(resources.loadModelAsync(path));
    }

    const auto ready = [](const auto &future) {
        return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };
    while (!window.shouldClose()) {
        resources.finishPendingLoads();
        std::size_t completed = static_cast<std::size_t>(ready(title)) +
                                static_cast<std::size_t>(ready(button)) +
                                static_cast<std::size_t>(ready(sky));
        for (const auto &model : models) {
            completed += static_cast<std::size_t>(ready(model));
        }
        if (completed == models.size() + 3) {
            break;
        }
        renderer.beginFrame();
        renderer.drawText("Loading assets " + std::to_string(completed) + "/" +
                              std::to_string(models.size() + 3),
                          {80.0f, 90.0f}, 40.0f, colors::White);
        renderer.endFrame();
    }
    if (window.shouldClose()) {
        return false;
    }
    title.get();
    button.get();
    sky.get();
    for (const auto &model : models) {
        model.get();
    }
    return true;
}

/// Declares the scenes of the client, but those of a game, which GameScenes declares.
void createScenes(rtype::engine::Engine &engine, rtype::render::ResourceManager &resources,
                  const rtype::engine::InterfaceStyle &style, rtype::render::Window &window,
                  rtype::client::Console &console, const rtype::engine::RendererMetrics &metrics,
                  rtype::client::GameScenes &games)
{
    const auto quit = [&window] { window.requestClose(); };
    engine.createScene(
        rtype::client::MainMenuScene, {.updatesWhenCovered = true, .onBack = [] {}},
        [&resources, &style, quit](rtype::engine::Engine &target, rtype::engine::SceneId scene) {
            rtype::client::buildMainMenu(target, scene, resources, style, quit);
        });
    rtype::client::createSubmenuScenes(
        engine, style, [&games] { games.play(std::make_unique<rtype::client::LocalSession>()); });
    engine.createScene(
        rtype::client::ConsoleScene, {.overlay = true},
        [&console, &metrics, &style](rtype::engine::Engine &target, rtype::engine::SceneId scene) {
            rtype::client::buildConsole(target, scene, console, metrics, style);
        });
}

/// Registers the systems of the client, in the order they run each frame, after the scenes.
/// @return The system that plays the games.
rtype::client::GameSessionSystem &registerSystems(rtype::engine::Engine &engine,
                                                  rtype::input::InputSource &devices,
                                                  const rtype::engine::RendererMetrics &metrics,
                                                  const rtype::engine::InterfaceStyle &style)
{
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<rtype::client::ConsoleSystem>(engine, devices);
    ecs.registerSystem<rtype::engine::InterfaceSystem>(ecs, engine.getScenes(), devices, metrics, style);
    // After the menus, which read the input first: Escape opens the pause menu before the game reads it
    auto &sessions =
        ecs.registerSystem<rtype::client::GameSessionSystem>(ecs, engine.getScenes(), engine.getControls());
    ecs.registerSystem<rtype::engine::WidgetTextSystem>();
    ecs.registerSystem<rtype::engine::BoundTextSystem>();
    ecs.registerSystem<rtype::engine::TweenSystem>();
    ecs.registerSystem<rtype::engine::SpinSystem>();
    ecs.registerSystem<rtype::client::FlightSystem>();
    ecs.registerSystem<rtype::client::IntroSystem>(engine.getScenes(), devices);
    ecs.registerSystem<rtype::engine::ModelAnimationSystem>();
    ecs.registerSystem<rtype::engine::HitboxSystem>(metrics, style);
    ecs.registerSystem<rtype::engine::ColumnLayoutSystem>(ecs);
    ecs.registerSystem<rtype::engine::AnchorSystem>(metrics);
    ecs.registerSystem<rtype::engine::ToastSystem>(ecs, metrics, style);
    ecs.registerSystem<rtype::ecs::TransformSystem>(ecs);
    return sessions;
}

void run()
{
    using namespace rtype::render;

    Window window({.fullscreen = true});
    rtype::engine::Engine engine;
    ResourceManager resources(window, applicationDirectory() / "assets", engine.assetLoading().queue());
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);

    if (!preloadAssets(window, renderer, resources)) {
        return;
    }

    const rtype::engine::InterfaceStyle style{
        .font = resources.loadFont(RTYPE_FONT_TITLE, 160),
        .button = {.font = resources.loadFont(RTYPE_FONT_BUTTON, 96)},
    };
    rtype::render::RenderQueries queries;
    rtype::render::RenderFrames frames;
    rtype::client::InputMailbox inputs;
    std::atomic<bool> stopping = false;
    std::exception_ptr logicFailure;
    const char *debugEnvironment = std::getenv("RTYPE_DEBUG_COLLIDERS");
    const bool initialColliderDebug = debugEnvironment && std::string_view(debugEnvironment) == "1";

    std::thread logic([&] {
        try {
            rtype::render::CommandRecorder recorder(renderer, queries);
            const rtype::engine::RendererMetrics metrics(recorder);
            rtype::client::SnapshotInput input([&](const std::string &text) {
                queries.run([&devices, text] { devices.setClipboardText(text); });
            });
            rtype::client::Console console;
            rtype::network::EventLoop network;
            engine.setInputSource(input);
            rtype::client::GameSessionSystem &sessions = registerSystems(engine, input, metrics, style);
            rtype::client::GameScenes games(engine, resources, metrics, style, sessions);
            rtype::client::OnlineScenes online(engine, network, games, metrics, style,
                                               applicationDirectory() / "server.pub");
            rtype::engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);
            rtype::engine::ColliderDebugRenderer colliderDebugRenderer(engine.getEcs(), engine.getScenes(),
                                                                       CollisionCellSize);
            createScenes(engine, resources, style, window, console, metrics, games);
            rtype::client::registerClientCommands(console, engine, window);
            engine.switchScene(rtype::client::MainMenuScene);
            bool showColliderDebug = initialColliderDebug;
            bool debugKeyWasDown = false;

            while (auto next = inputs.take()) {
                const float elapsed = next->elapsed;
                input.update(std::move(*next));
                const bool debugKeyDown = input.isKeyDown(rtype::input::Key::F3);
                if (debugKeyDown && !debugKeyWasDown) {
                    showColliderDebug = !showColliderDebug;
                }
                debugKeyWasDown = debugKeyDown;
                network.poll();
                engine.update(elapsed);
                sceneRenderer.draw(recorder, rtype::engine::Layer::World);
                if (showColliderDebug) {
                    colliderDebugRenderer.draw(recorder);
                }
                sceneRenderer.draw(recorder, rtype::engine::Layer::Interface);
                frames.publish(recorder.take());
            }
        } catch (...) {
            if (!stopping.load()) {
                logicFailure = std::current_exception();
                window.requestClose();
            }
        }
    });

    rtype::render::RenderCommands current;
    std::exception_ptr renderFailure;
    try {
        while (!window.shouldClose()) {
            const auto queryDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4);
            do {
                resources.finishPendingLoads();
                queries.drain();
                if (auto next = frames.take()) {
                    current = std::move(*next);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } while (std::chrono::steady_clock::now() < queryDeadline && !window.shouldClose());
            if (window.shouldClose()) {
                break;
            }
            renderer.beginFrame();
            for (const auto &command : current) {
                command(renderer);
            }
            renderer.endFrame();
            inputs.publish(rtype::client::InputFrame::capture(devices, window.frameTime()));
        }
    } catch (...) {
        renderFailure = std::current_exception();
    }
    stopping = true;
    inputs.stop();
    queries.stop();
    logic.join();
    if (renderFailure) {
        std::rethrow_exception(renderFailure);
    }
    if (logicFailure) {
        std::rethrow_exception(logicFailure);
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
