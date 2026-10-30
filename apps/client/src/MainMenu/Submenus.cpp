#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "MainMenu/UiShowcase.hpp"
#include "rtype/engine/Component/Drawing.hpp"
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
    std::string key;
    std::function<void()> onPress;
};

/// Space between the header and the first button, and between the tops of two buttons.
constexpr float buttonsTop = 260.0f;
constexpr float buttonsSpacing = 103.0f;

void createSubmenuScene(engine::Engine &engine, const engine::InterfaceStyle &style, const char *name,
                        localization::Localization &translations, const std::string &titleKey,
                        std::vector<SubmenuButton> buttons = {})
{
    engine.createScene(
        name, {.overlay = true, .hidesInterfaceBelow = true},
        [&style, &translations, titleKey, buttons = std::move(buttons)](engine::Engine &target,
                                                                        engine::SceneId scene) {
            engine::InterfaceBuilder interface(target.getEcs(), scene, style);
            interface.boundHeader([&translations, titleKey] { return translations.text(titleKey); },
                                  [&target] { target.popScene(); },
                                  {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
            float top = buttonsTop;
            for (const SubmenuButton &button : buttons) {
                const ecs::entity_t entity =
                    interface.button(translations.text(button.key), button.onPress,
                                     {engine::AnchorPoint::TopLeft, screenMargin, top});
                target.getEcs().addComponent<engine::BoundText>(
                    entity, [&translations, key = button.key] { return translations.text(key); });
                top += buttonsSpacing;
            }
        });
}

void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations)
{
    engine.createScene(
        SettingsMenuScene, {.overlay = true, .hidesInterfaceBelow = true},
        [&style, &translations](engine::Engine &target, engine::SceneId scene) {
            engine::InterfaceBuilder interface(target.getEcs(), scene, style);
            interface.boundHeader([&translations] { return translations.text("menu.settings"); },
                                  [&target] { target.popScene(); },
                                  {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
            const auto label = [&translations] {
                const std::string key =
                    translations.language() == "fr" ? "language.french" : "language.english";
                return translations.text("settings.language") + ": " + translations.text(key);
            };
            const ecs::entity_t choice = interface.button(
                label(),
                [&translations] { translations.setLanguage(translations.language() == "fr" ? "en" : "fr"); },
                {engine::AnchorPoint::TopLeft, screenMargin, buttonsTop});
            target.getEcs().addComponent<engine::BoundText>(choice, label);
            const ecs::entity_t showcase = interface.button(
                translations.text("showcase.open"), [&target] { target.pushScene(UiShowcaseScene); },
                {engine::AnchorPoint::TopLeft, screenMargin, buttonsTop + buttonsSpacing});
            target.getEcs().addComponent<engine::BoundText>(
                showcase, [&translations] { return translations.text("showcase.open"); });
        });
}

} // namespace

void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations, std::function<void()> playSolo,
                         std::function<void()> openHangar)
{
    createSubmenuScene(engine, style, PlayMenuScene, translations, "menu.play",
                       {{"menu.solo", std::move(playSolo)}, {"menu.ships", std::move(openHangar)}});
    createSettingsScene(engine, style, translations);
    createUiShowcaseScene(engine, style, translations);
}

} // namespace rtype::client
