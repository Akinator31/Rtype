#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// A button of a submenu, in a column on the left, under its header.
struct SubmenuButton {
    std::string text;
    std::function<void()> onPress;
};

/// Space between the header and the first button, and between the tops of two buttons.
constexpr float buttonsTop = 260.0f;
constexpr float buttonsSpacing = 103.0f;

void createSubmenuScene(engine::Engine &engine, const engine::InterfaceStyle &style, const char *name,
                        const std::string &title, std::vector<SubmenuButton> buttons = {})
{
    engine.createScene(
        name, {.overlay = true, .hidesInterfaceBelow = true},
        [&style, title, buttons = std::move(buttons)](engine::Engine &target, engine::SceneId scene) {
            engine::InterfaceBuilder interface(target.getEcs(), scene, style);
            interface.header(title, [&target] { target.popScene(); },
                             {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
            float top = buttonsTop;
            for (const SubmenuButton &button : buttons) {
                interface.button(button.text, button.onPress,
                                 {engine::AnchorPoint::TopLeft, screenMargin, top});
                top += buttonsSpacing;
            }
        });
}

} // namespace

void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                         std::function<void()> playSolo)
{
    createSubmenuScene(engine, style, PlayMenuScene, "Play", {{"Solo", std::move(playSolo)}});
    createSubmenuScene(engine, style, OnlineMenuScene, "Online");
    createSubmenuScene(engine, style, SettingsMenuScene, "Settings");
}

} // namespace rtype::client
