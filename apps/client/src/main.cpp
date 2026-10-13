#include "MainMenu/MainMenu.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Engine.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/log/Logger.hpp"
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
                  const rtype::engine::InterfaceStyle &style, rtype::render::Window &window)
{
    const auto quit = [&window] { window.requestClose(); };
    engine.createScene(
        rtype::client::MainMenuScene, {},
        [&resources, &style, quit](rtype::engine::Engine &target, rtype::engine::SceneId scene) {
            rtype::client::buildMainMenu(target, scene, resources, style, quit);
        });
    engine.createScene(GameScene, {.updatesWhenCovered = true});
}

/// Registers the systems of the client, in the order they run each frame, after the scenes.
void registerSystems(rtype::engine::Engine &engine, rtype::input::Devices &devices,
                     const rtype::engine::RendererMetrics &metrics,
                     const rtype::engine::InterfaceStyle &style)
{
    rtype::ecs::Ecs &ecs = engine.getEcs();
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
        .font = resources.loadFont("fonts/FiraSans-Black.ttf", 160),
        .button = {.font = resources.loadFont("fonts/Inter-SemiBold.ttf", 96)},
    };
    const rtype::engine::RendererMetrics metrics(renderer);

    rtype::engine::Engine engine;
    engine.setInputSource(devices);
    registerSystems(engine, devices, metrics, style);
    rtype::engine::SceneRenderer sceneRenderer(engine.getEcs(), engine.getScenes(), style);

    createScenes(engine, resources, style, window);
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
