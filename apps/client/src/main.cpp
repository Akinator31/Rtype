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

    // Declaration order matters: resources and renderer are destroyed before the window they depend on
    const Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);

    const float titleSize = 160.0f;
    const FontId font = resources.loadFont("fonts/FiraSans-Black.ttf", static_cast<int>(titleSize));
    const TextureId texture = resources.loadTexture("images/big_mama.png");
    const std::string title = "R-Type";

    while (!window.shouldClose()) {
        const Vector2 game = renderer.gameSize();
        const Vector2 titleArea = renderer.measureText(font, title, titleSize);

        renderer.beginFrame(colors::White);
        renderer.drawTexture(texture, {200.0f, 200.0f});
        renderer.drawText(font, title, {(game.x - titleArea.x) / 2.0f, (game.y - titleArea.y) / 2.0f},
                          titleSize, colors::White);
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
