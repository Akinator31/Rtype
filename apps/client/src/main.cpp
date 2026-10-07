#include "Controls.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/input/Devices.hpp"
#include "rtype/input/TextInput.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/render/Paths.hpp"
#include "rtype/render/Renderer.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/render/Window.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <optional>
#include <string>

namespace
{

void run()
{
    using namespace rtype::render;
    using rtype::client::Action;
    using rtype::client::MenuAction;

    const rtype::log::Logger logger("client");

    const Window window;
    ResourceManager resources(window, applicationDirectory() / "assets");
    Renderer renderer(window, resources);
    rtype::input::Devices devices(window);
    rtype::input::ActionMap<Action> controls = rtype::client::defaultControls();
    rtype::input::ActionMap<MenuAction> menuControls = rtype::client::defaultMenuControls();

    const float titleSize = 160.0f;
    const FontId font = resources.loadFont("fonts/FiraSans-Black.ttf", static_cast<int>(titleSize));
    const TextureId texture = resources.loadTexture("images/big_mama.png");
    const std::string title = "R-Type";
    const std::string greeting = rtype::ecs::helloWorld();
    const float greetingSize = 48.0f;

    // Demo of the controls: the image moves with the movement actions, Shoot counts the shots, Menu opens a
    // placeholder menu, navigated with the menu actions, during which the game actions are ignored
    const float speed = 600.0f;
    const float imageSize = 300.0f;
    Vector2 position{200.0f, 200.0f};
    int shots = 0;
    bool menuOpen = false;
    const std::array<std::string, 3> menuItems{"Resume", "Name: ", "Quit"};
    const std::size_t nameItem = 1;
    std::size_t selectedItem = 0;
    // Area of each item on screen, kept from the previous frame to know which item is under the mouse pointer
    std::array<Rectangle, 3> itemAreas{};
    rtype::input::MousePosition previousMouse = devices.mousePosition();
    // The name is edited while its item is selected
    rtype::input::TextInput name;
    name.setText("Player");
    name.setMaxLength(16);
    name.setFilter([](char32_t c) { return (c >= U'0' && c <= U'9') || c == U'.' || c == U':'; });

    while (!window.shouldClose()) {
        const Vector2 game = renderer.gameSize();

        // Both maps are updated every frame, but only the one matching the state at the start of the frame is
        // read: Escape opens the menu in game and closes it in the menu, never both in the same frame
        const bool menuWasOpen = menuOpen;
        controls.update(devices, window.frameTime());
        menuControls.update(devices, window.frameTime());
        if (!menuWasOpen && controls.wasPressed(Action::Menu)) {
            menuOpen = true;
            selectedItem = 0;
        }

        // The pointer is converted to game units: the game is scaled and may have black bars around it
        const rtype::input::MousePosition mouse = devices.mousePosition();
        const bool mouseMoved = mouse.x != previousMouse.x || mouse.y != previousMouse.y;
        previousMouse = mouse;
        const Vector2 pointer = renderer.windowToGame({mouse.x, mouse.y});
        std::optional<std::size_t> hoveredItem;
        for (std::size_t item = 0; item < itemAreas.size(); ++item) {
            const Rectangle &area = itemAreas[item];
            if (pointer.x >= area.x && pointer.x < area.x + area.width && pointer.y >= area.y &&
                pointer.y < area.y + area.height) {
                hoveredItem = item;
            }
        }

        const auto activate = [&](std::size_t item) {
            if (item == 0) {
                menuOpen = false;
            } else if (item == nameItem) {
                logger.info("Name set to \"{}\"", name.text());
            } else {
                logger.info("Menu item \"{}\" is not implemented yet", menuItems[item]);
            }
        };
        if (menuWasOpen) {
            // The mouse only selects when it moves: a still pointer over an item must not prevent choosing
            // another one with the keyboard
            if (mouseMoved && hoveredItem) {
                selectedItem = *hoveredItem;
            }
            // wasRepeated(): holding Up or Down keeps moving the selection after a short delay
            if (menuControls.wasRepeated(MenuAction::Up)) {
                selectedItem = (selectedItem + menuItems.size() - 1) % menuItems.size();
            }
            if (menuControls.wasRepeated(MenuAction::Down)) {
                selectedItem = (selectedItem + 1) % menuItems.size();
            }
            if (selectedItem == nameItem) {
                name.update(devices, window.frameTime());
                if (name.wasChanged()) {
                    logger.debug("Name changed to \"{}\"", name.text());
                }
            }
            if (menuControls.wasPressed(MenuAction::Back)) {
                menuOpen = false;
            } else if (menuControls.wasPressed(MenuAction::Confirm)) {
                activate(selectedItem);
            } else if (menuControls.wasPressed(MenuAction::Click) && hoveredItem) {
                selectedItem = *hoveredItem;
                activate(*hoveredItem);
            }
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
        if (!menuWasOpen && controls.wasPressed(Action::Shoot)) {
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
            const std::string menuHint = "Up / Down to choose, Enter to confirm, Escape to go back";
            const Vector2 menuTitleArea = renderer.measureText(font, menuTitle, titleSize);
            const Vector2 menuHintArea = renderer.measureText(menuHint, greetingSize);
            renderer.drawRectangle({0.0f, 0.0f, game.x, game.y}, {0, 0, 0, 180});
            renderer.drawText(font, menuTitle,
                              {(game.x - menuTitleArea.x) / 2.0f, game.y / 2.0f - menuTitleArea.y}, titleSize,
                              colors::White);
            renderer.drawText(menuHint, {(game.x - menuHintArea.x) / 2.0f, game.y - menuHintArea.y - 40.0f},
                              greetingSize, colors::White);

            renderer.drawRectangle({100.0f, 100.0f, 50.0f, 90.0f}, colors::Yellow);
            renderer.drawRectangleOutline({100.0f, 100.0f, 50.0f, 90.0f}, 10.0f, colors::Green);

            renderer.drawLine({100.0f, 540.0f}, {1800.0f, 540.0f}, 8.0f, colors::Red);
            renderer.drawCircle({960.0f, 240.0f}, 50.0f, colors::Red);
            renderer.drawTriangle({150.0f, 540.0f}, {100.0f, 510.0f}, {100.0f, 570.0f}, colors::Blue);

            const float itemSize = 64.0f;
            const float itemSpacing = 100.0f;
            for (std::size_t item = 0; item < menuItems.size(); ++item) {
                const bool selected = item == selectedItem;
                const std::string label = item == nameItem ? menuItems[item] + name.text() : menuItems[item];
                const Vector2 itemArea = renderer.measureText(font, label, itemSize);
                const Vector2 itemPosition{(game.x - itemArea.x) / 2.0f,
                                           game.y / 2.0f + static_cast<float>(item) * itemSpacing};
                itemAreas[item] = {itemPosition.x - 30.0f, itemPosition.y - 10.0f, itemArea.x + 60.0f,
                                   itemArea.y + 20.0f};
                if (selected) {
                    renderer.drawRectangleOutline(itemAreas[item], 4.0f, colors::Yellow);
                }
                // While the name is edited: selection behind the text, cursor after it. Their positions are
                // the widths of the text before them.
                const bool editing = selected && item == nameItem;
                const float textStart =
                    itemPosition.x + renderer.measureText(font, menuItems[item], itemSize).x;
                const auto offsetOf = [&](std::size_t byte) {
                    return textStart + renderer.measureText(font, name.text().substr(0, byte), itemSize).x;
                };
                if (editing && !name.selection().empty()) {
                    const float begin = offsetOf(name.selection().begin);
                    renderer.drawRectangle(
                        {begin, itemPosition.y, offsetOf(name.selection().end) - begin, itemArea.y},
                        {0, 121, 241, 160});
                }
                renderer.drawText(font, label, itemPosition, itemSize,
                                  selected ? colors::Yellow : colors::White);
                if (editing) {
                    const float cursorX = offsetOf(name.cursor());
                    renderer.drawLine({cursorX, itemPosition.y}, {cursorX, itemPosition.y + itemArea.y}, 3.0f,
                                      colors::White);
                }
            }
        }
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
