#include "rtype/ecs/Ecs.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/render/Window.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace
{

void run()
{
    using namespace rtype::render;

    const Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);

    const float titleSize = 160.0f;
    const FontId font = resources.loadFont("fonts/FiraSans-Black.ttf", static_cast<int>(titleSize));
    const TextureId texture = resources.loadTexture("images/big_mama.png");
    const std::string title = "R-Type";
    const std::string greeting = rtype::ecs::helloWorld();
    const float greetingSize = 48.0f;

    while (!window.shouldClose()) {
        const Vector2 game = renderer.gameSize();
        const Vector2 titleArea = renderer.measureText(font, title, titleSize);

        renderer.beginFrame(colors::White);
        renderer.drawTexture(texture, {200.0f, 200.0f});
        renderer.drawText(font, title, {(game.x - titleArea.x) / 2.0f, (game.y - titleArea.y) / 2.0f},
                          titleSize, colors::White);
        const Vector2 greetingArea = renderer.measureText(greeting, greetingSize);
        renderer.drawText(greeting, {(game.x - greetingArea.x) / 2.0f, game.y - greetingArea.y - 40.0f},
                          greetingSize, colors::Blue);
        renderer.drawFps({10.0f, 10.0f});
        renderer.endFrame();
    }
}

} // namespace

int main()
{
    try {
        run();
    } catch (const std::exception &error) {
        std::cerr << "r-type_client: " << error.what() << std::endl;
        return 84;
    }
    return 0;
}
