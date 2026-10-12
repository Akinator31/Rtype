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

constexpr const char *MainMenuScene = "main_menu";
constexpr const char *GameScene = "game";

/// Declares the scenes of the client. They are empty for now.
void createScenes(rtype::engine::Engine &engine)
{
    engine.createScene(MainMenuScene);
    engine.createScene(GameScene, {.updatesWhenCovered = true});
}

void run()
{
    using namespace rtype::render;

    Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);

    rtype::engine::Engine engine;
    engine.setInputSource(devices);
    createScenes(engine);
    engine.switchScene(MainMenuScene);

    while (!window.shouldClose()) {
        engine.update(window.frameTime());

        renderer.beginFrame();
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
