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
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/physics/Physics.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Window.hpp"

#include <cstdlib>
#include <exception>
#include <memory>
#include <string_view>

namespace
{

constexpr float CollisionCellSize = 64.0f;

/// Declares the scenes of the client, but those of a game, which GameScenes declares.
void createScenes(rtype::engine::Engine &engine, rtype::render::ResourceManager &resources,
                  const rtype::engine::InterfaceStyle &style, rtype::render::Window &window,
                  rtype::client::Console &console, const rtype::engine::RendererMetrics &metrics,
                  rtype::client::GameScenes &games)
{
    const auto quit = [&window] { window.requestClose(); };
    engine.createScene(
        rtype::client::MainMenuScene, {.updatesWhenCovered = true},
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
                                                  rtype::input::Devices &devices,
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
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);

    const rtype::engine::InterfaceStyle style{
        .font = resources.loadFont(RTYPE_FONT_TITLE, 160),
        .button = {.font = resources.loadFont(RTYPE_FONT_BUTTON, 96)},
    };
    const rtype::engine::RendererMetrics metrics(renderer);

    rtype::client::Console console;
    rtype::engine::Engine engine;
    engine.setInputSource(devices);
    rtype::client::GameSessionSystem &sessions = registerSystems(engine, devices, metrics, style);
    rtype::client::GameScenes games(engine, resources, metrics, style, sessions);
    rtype::engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);
    rtype::engine::ColliderDebugRenderer colliderDebugRenderer(engine.getEcs(), engine.getScenes(),
                                                               CollisionCellSize);
    const char *debugEnvironment = std::getenv("RTYPE_DEBUG_COLLIDERS");
    bool showColliderDebug = debugEnvironment && std::string_view(debugEnvironment) == "1";
    bool debugKeyWasDown = false;

    createScenes(engine, resources, style, window, console, metrics, games);
    rtype::client::registerClientCommands(console, engine, window);
    engine.switchScene(rtype::client::MainMenuScene);

    while (!window.shouldClose()) {
        const bool debugKeyDown = devices.isKeyDown(rtype::input::Key::F3);
        if (debugKeyDown && !debugKeyWasDown) {
            showColliderDebug = !showColliderDebug;
        }
        debugKeyWasDown = debugKeyDown;
        engine.update(window.frameTime());
        renderer.beginFrame();
        sceneRenderer.draw(renderer, rtype::engine::Layer::World);
        if (showColliderDebug) {
            colliderDebugRenderer.draw(renderer);
        }
        sceneRenderer.draw(renderer, rtype::engine::Layer::Interface);
        renderer.endFrame();
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
