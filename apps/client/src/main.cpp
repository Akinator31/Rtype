#include "Assets.hpp"
#include "Commands/Commands.hpp"
#include "Console/Console.hpp"
#include "Console/ConsoleScene.hpp"
#include "Console/ConsoleSystem.hpp"
#include "MainMenu/MainMenu.hpp"
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

#include <exception>

namespace
{

constexpr const char *GameScene = "game";

/// Declares the scenes of the client. The game scene is empty for now.
void createScenes(rtype::engine::Engine &engine, rtype::render::ResourceManager &resources,
                  const rtype::engine::InterfaceStyle &style, rtype::render::Window &window,
                  rtype::client::Console &console, const rtype::engine::RendererMetrics &metrics)
{
    const auto quit = [&window] { window.requestClose(); };
    engine.createScene(
        rtype::client::MainMenuScene, {},
        [&resources, &style, quit](rtype::engine::Engine &target, rtype::engine::SceneId scene) {
            rtype::client::buildMainMenu(target, scene, resources, style, quit);
        });
    engine.createScene(GameScene, {.updatesWhenCovered = true});
    engine.createScene(
        rtype::client::ConsoleScene, {.overlay = true},
        [&console, &metrics, &style](rtype::engine::Engine &target, rtype::engine::SceneId scene) {
            rtype::client::buildConsole(target, scene, console, metrics, style);
        });
}

/// Registers the systems of the client, in the order they run each frame, after the scenes.
void registerSystems(rtype::engine::Engine &engine, rtype::input::Devices &devices,
                     const rtype::engine::RendererMetrics &metrics,
                     const rtype::engine::InterfaceStyle &style)
{
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<rtype::client::ConsoleSystem>(engine, devices);
    ecs.registerSystem<rtype::engine::InterfaceSystem>(ecs, engine.getScenes(), devices, metrics, style);
    ecs.registerSystem<rtype::engine::WidgetTextSystem>();
    ecs.registerSystem<rtype::engine::BoundTextSystem>();
    ecs.registerSystem<rtype::engine::TweenSystem>();
    ecs.registerSystem<rtype::engine::HitboxSystem>(metrics, style);
    ecs.registerSystem<rtype::engine::ColumnLayoutSystem>(ecs);
    ecs.registerSystem<rtype::engine::AnchorSystem>(metrics);
    ecs.registerSystem<rtype::ecs::TransformSystem>(ecs);
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
    registerSystems(engine, devices, metrics, style);
    rtype::engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);

    createScenes(engine, resources, style, window, console, metrics);
    rtype::client::registerClientCommands(console, engine, window);
    engine.switchScene(rtype::client::MainMenuScene);

    while (!window.shouldClose()) {
        engine.update(window.frameTime());
        renderer.beginFrame();
        sceneRenderer.draw(renderer);
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
