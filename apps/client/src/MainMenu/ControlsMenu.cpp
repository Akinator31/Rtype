#include "MainMenu/ControlsMenu.hpp"

#include "Controls/KeyCaptureSystem.hpp"
#include "MainMenu/Column.hpp"
#include "Settings/ControlSettings.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/input/Devices.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace rtype::client
{

namespace
{

/// The texts of the scene that asks for a key, above and under the middle of the screen.
constexpr float captureTextOffset = 50.0f;

/// The action whose key is being chosen, shared by the Controls page and the scene that asks for the key.
using ChosenAction = std::shared_ptr<const GameAction *>;

std::string actionKey(const GameAction &action)
{
    return std::string(ControlsSettingsPrefix).append(action.name);
}

/// The name of a key for the player: the character printed on it on their layout ("Z" for Key::W on AZERTY),
/// or its translated name for a key without one ("Espace").
std::string keyText(const localization::Localization &translations, input::Key key)
{
    if (std::string label = input::keyLabel(key); !label.empty()) {
        return label;
    }
    const std::string name(input::keyName(key));
    const std::string translationKey = "key." + name;
    const std::string translated = translations.text(translationKey);
    return translated != translationKey ? translated : name;
}

/// "Up / Z", the keys of an action, or "None".
std::string keysText(const localization::Localization &translations, const std::vector<input::Key> &keys)
{
    if (keys.empty()) {
        return translations.text("controls.none");
    }
    std::string text;
    for (const input::Key key : keys) {
        text.append(text.empty() ? "" : " / ").append(keyText(translations, key));
    }
    return text;
}

/// "Fire: Space": opens the scene that asks for a new key for the action.
void addActionButton(engine::InterfaceBuilder &interface, engine::Engine &engine,
                     const localization::Localization &translations, const GameAction &action, int row,
                     const ChosenAction &chosen)
{
    const engine::Controls &controls = engine.getControls();
    const auto label = [&translations, &controls, &action] {
        return choiceLabel(translations, actionKey(action),
                           keysText(translations, keysOf(controls, action.control)));
    };
    const auto choose = [&engine, &action, chosen] {
        *chosen = &action;
        engine.pushScene(KeyCaptureScene);
    };
    boundButton(interface, engine.getEcs(), label, choose, rowTop(row));
}

void buildControlsPage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                       const localization::Localization &translations, const ChosenAction &chosen,
                       const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    interface.boundHeader([&translations] { return translations.text("settings.controls"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    int row = 0;
    for (const GameAction &action : GameActions) {
        addActionButton(interface, engine, translations, action, row++, chosen);
    }
    boundButton(
        interface, ecs, [&translations] { return translations.text("controls.reset"); },
        [&engine] { engine.getControls().resetDefaults(); }, rowTop(row));
}

/// "Press a key for: Fire", and how to cancel. The first key pressed goes to the action, and the scene
/// closes.
void buildKeyCapture(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                     const localization::Localization &translations, const GameAction &action)
{
    engine::InterfaceBuilder interface(engine.getEcs(), scene, style);
    const ecs::entity_t question =
        interface.title(translations.text("controls.press") + " " + translations.text(actionKey(action)),
                        {engine::AnchorPoint::Center, 0.0f, -captureTextOffset});
    interface.text(translations.text("controls.cancel"),
                   {engine::AnchorPoint::Center, 0.0f, captureTextOffset});
    engine.getEcs().addComponent<KeyCapture>(question,
                                             KeyCapture{.onKey = [&engine, &action](input::Key key) {
                                                 assignKey(engine.getControls(), action.control, key);
                                                 engine.popScene();
                                             }});
}

} // namespace

void createControlsScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                          localization::Localization &translations)
{
    const auto chosen = std::make_shared<const GameAction *>(&GameActions.front());
    // Written when the page closes, not at each key
    const auto close = [&engine] {
        keepControlSettings(engine.getSettings(), engine.getControls());
        writeSettings(engine.getSettings());
        engine.popScene();
    };
    engine.createScene(ControlsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, &translations, chosen, close](engine::Engine &target, engine::SceneId scene) {
                           buildControlsPage(target, scene, style, translations, chosen, close);
                       });
    engine.createScene(KeyCaptureScene, {.overlay = true, .hidesInterfaceBelow = true},
                       [&style, &translations, chosen](engine::Engine &target, engine::SceneId scene) {
                           buildKeyCapture(target, scene, style, translations, **chosen);
                       });
}

} // namespace rtype::client
