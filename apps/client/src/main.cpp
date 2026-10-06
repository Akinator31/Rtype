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

    // Demo of the controls: the image moves with the movement actions, Shoot counts the shots
    const float speed = 600.0f;
    const float imageSize = 300.0f;
    Vector2 position{200.0f, 200.0f};
    int shots = 0;

    while (!window.shouldClose()) {
        const Vector2 game = renderer.gameSize();

        controls.update(devices);
        Vector2 direction{controls.strength(Action::MoveRight) - controls.strength(Action::MoveLeft),
                          controls.strength(Action::MoveDown) - controls.strength(Action::MoveUp)};
        // Moving diagonally must not be faster than moving straight
        const float length = std::hypot(direction.x, direction.y);
        if (length > 1.0f) {
            direction = {direction.x / length, direction.y / length};
        }
        position.x =
            std::clamp(position.x + direction.x * speed * window.frameTime(), 0.0f, game.x - imageSize);
        position.y =
            std::clamp(position.y + direction.y * speed * window.frameTime(), 0.0f, game.y - imageSize);
        if (controls.wasPressed(Action::Shoot)) {
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
