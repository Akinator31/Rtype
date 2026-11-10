#include "MainMenu/Submenus.hpp"

#include "Lore/LoreScene.hpp"
#include "MainMenu/Column.hpp"
#include "MainMenu/UiShowcase.hpp"
#include "Quests/QuestsMenu.hpp"

#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// A button of a submenu, in a column on the left, under its header.
struct SubmenuButton {
    std::string key;
    std::function<void()> onPress;
};

/// Space between the tops of two buttons.
constexpr float buttonsSpacing = 103.0f;

void createSubmenuScene(engine::Engine &engine, const engine::InterfaceStyle &style, const char *name,
                        localization::Localization &translations, const std::string &titleKey,
                        std::vector<SubmenuButton> buttons = {})
{
    engine.createScene(name, {.overlay = true, .hidesInterfaceBelow = true},
                       [&style, &translations, titleKey,
                        buttons = std::move(buttons)](engine::Engine &target, engine::SceneId scene) {
                           engine::InterfaceBuilder interface(target.getEcs(), scene, style);
                           interface.boundHeader(
                               [&translations, titleKey] { return translations.text(titleKey); },
                               [&target] { target.popScene(); },
                               {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
                           float top = columnTop;
                           for (const SubmenuButton &button : buttons) {
                               boundButton(
                                   interface, target.getEcs(),
                                   [&translations, key = button.key] { return translations.text(key); },
                                   button.onPress, top);
                               top += buttonsSpacing;
                           }
                       });
}

} // namespace

void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations, std::function<void()> playSolo,
                         std::function<void()> openHangar)
{
    createSubmenuScene(engine, style, PlayMenuScene, translations, "menu.play",
                       {{"menu.solo", std::move(playSolo)},
                        {"menu.ships", std::move(openHangar)},
                        {"menu.lore", [&engine] { engine.pushScene(LoreScene); }},
                        {"menu.quests", [&engine] { engine.pushScene(QuestsMenuScene); }}});
    createUiShowcaseScene(engine, style, translations);
}

} // namespace rtype::client
