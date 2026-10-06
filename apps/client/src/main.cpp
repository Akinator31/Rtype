#include "Controls.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace
{

void run()
{
    using namespace rtype::render;
    using rtype::client::Action;

    const Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);
    const rtype::input::Devices devices(window);
    rtype::input::ActionMap<Action> controls = rtype::client::defaultControls();

    const float titleSize = 160.0f;
    const FontId font = resources.loadFont("fonts/FiraSans-Black.ttf", static_cast<int>(titleSize));
    const TextureId texture = resources.loadTexture("images/big_mama.png");
    const std::string title = "R-Type";
    const std::string greeting = rtype::ecs::helloWorld();
    const float greetingSize = 48.0f;

    // Demo of the controls: the image moves with the movement actions, Shoot counts the shots, Menu opens a
    // placeholder menu during which the other actions are ignored
    const float speed = 600.0f;
    const float imageSize = 300.0f;
    Vector2 position{200.0f, 200.0f};
    int shots = 0;
    bool menuOpen = false;

    while (!window.shouldClose()) {
        const Vector2 game = renderer.gameSize();

        controls.update(devices);
        if (controls.wasPressed(Action::Menu)) {
            menuOpen = !menuOpen;
        }
        Vector2 direction{controls.strength(Action::MoveRight) - controls.strength(Action::MoveLeft),
                          controls.strength(Action::MoveDown) - controls.strength(Action::MoveUp)};
        // Moving diagonally must not be faster than moving straight
        const float length = std::hypot(direction.x, direction.y);
        if (menuOpen) {
            direction = {0.0f, 0.0f};
        } else if (length > 1.0f) {
            direction = {direction.x / length, direction.y / length};
        }
        position.x =
            std::clamp(position.x + direction.x * speed * window.frameTime(), 0.0f, game.x - imageSize);
        position.y =
            std::clamp(position.y + direction.y * speed * window.frameTime(), 0.0f, game.y - imageSize);
        if (!menuOpen && controls.wasPressed(Action::Shoot)) {
            ++shots;
        }
        const Vector2 titleArea = renderer.measureText(font, title, titleSize);

        renderer.beginFrame(colors::White);
        const Vector2 textureArea = renderer.textureSize(texture);
        renderer.drawTexture(texture, {0.0f, 0.0f, textureArea.x, textureArea.y},
                             {position.x, position.y, imageSize, imageSize});
        renderer.drawText(font, title, {(game.x - titleArea.x) / 2.0f, (game.y - titleArea.y) / 2.0f},
                          titleSize, colors::White);
        const Vector2 greetingArea = renderer.measureText(greeting, greetingSize);
        renderer.drawText(greeting, {(game.x - greetingArea.x) / 2.0f, game.y - greetingArea.y - 40.0f},
                          greetingSize, colors::Blue);
        renderer.drawText("Shots: " + std::to_string(shots), {10.0f, 50.0f}, 40.0f, colors::Red);
        renderer.drawFps({10.0f, 10.0f});
        if (menuOpen) {
            const std::string menuTitle = "Menu";
            const std::string menuHint = "Press Escape or Start to go back";
            const Vector2 menuTitleArea = renderer.measureText(font, menuTitle, titleSize);
            const Vector2 menuHintArea = renderer.measureText(menuHint, greetingSize);
            renderer.drawRectangle({0.0f, 0.0f, game.x, game.y}, {0, 0, 0, 180});
            renderer.drawText(font, menuTitle,
                              {(game.x - menuTitleArea.x) / 2.0f, game.y / 2.0f - menuTitleArea.y}, titleSize,
                              colors::White);
            renderer.drawText(menuHint, {(game.x - menuHintArea.x) / 2.0f, game.y / 2.0f + 40.0f},
                              greetingSize, colors::White);

            renderer.drawLine({100.0f, 540.0f}, {1800.0f, 540.0f}, 8.0f, colors::Red);
            renderer.drawCircle({960.0f, 240.0f}, 50.0f, colors::Red);
            renderer.drawTriangle({150.0f, 540.0f}, {100.0f, 510.0f}, {100.0f, 570.0f}, colors::Blue);
        }
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
