#include "MainMenu/ControlsMenu.hpp"

#include "Controls/KeyCaptureSystem.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/ControlSettings.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/input/Devices.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// The table of the controls, from the left margin: its width, and where its columns of keys and of gamepad
/// buttons start.
constexpr float tableWidth = 1752.0f;
constexpr float keysColumn = 688.0f;
constexpr float buttonsColumn = 1248.0f;
/// The heads of the columns, the rule under them, and the top of the first group.
constexpr float headsTop = 192.0f;
constexpr float headsRuleTop = 230.0f;
constexpr float headsRuleThickness = 2.0f;
constexpr render::Color headsRule{255, 255, 255, 36};
constexpr float groupsTop = 256.0f;
/// Space between the groups of the table, smaller than on the Settings page: the rows are lower.
constexpr float groupGap = 16.0f;
/// The first action of the group of the fire: the first ones move the ship.
constexpr std::size_t fireGroupStart = 4;
/// Space between the table and the button of the default keys, where the banner of a choice of key is too.
constexpr float bottomGap = 32.0f;
/// The banner of a choice of key: its title, and the line under it.
constexpr float bannerTitleSize = 26.0f;
constexpr float bannerTitleSpacing = 5.0f;
constexpr float bannerLineGap = 44.0f;

/// The choice of a key, shared by the Controls page and the scene that asks for the key.
struct Capture {
    /// The action whose key is being chosen, or nothing.
    const GameAction *action = nullptr;
    /// The key pressed, while it waits for a confirmation because another action has it.
    std::optional<input::Key> key{};
    /// That other action.
    const GameAction *owner = nullptr;
    /// The button of the default keys, hidden while a key is chosen: the banner takes its place.
    ecs::entity_t resetButton = 0;
};
using SharedCapture = std::shared_ptr<Capture>;

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

/// The keys of a row: those of its action, the key waited for or chosen while the player chooses one for it,
/// and the key that would leave it in red.
std::vector<engine::KeyCap> keysOfRow(const engine::Controls &controls,
                                      const localization::Localization &translations,
                                      const GameAction &action, const Capture &capture)
{
    if (capture.action == &action) {
        if (capture.key) {
            return {{keyText(translations, *capture.key), engine::KeyCapState::New}};
        }
        return {{"", engine::KeyCapState::Waiting}};
    }
    std::vector<engine::KeyCap> keys;
    for (const input::Key key : keysOf(controls, action.control)) {
        const bool taken = capture.owner == &action && capture.key == key;
        keys.push_back({keyText(translations, key),
                        taken ? engine::KeyCapState::Conflict : engine::KeyCapState::Normal});
    }
    if (keys.empty()) {
        keys.push_back({translations.text("controls.none"), engine::KeyCapState::Normal});
    }
    return keys;
}

/// The row of an action: pressing it asks for a new key, in the scene over the page.
void addActionRow(engine::InterfaceBuilder &interface, engine::Engine &engine,
                  const localization::Localization &translations, const GameAction &action,
                  const SharedCapture &capture, float top)
{
    const engine::Controls &controls = engine.getControls();
    engine::KeyRow row{
        .keys = [&controls, &translations, &action,
                 capture] { return keysOfRow(controls, translations, action, *capture); },
        .buttons =
            [&controls, &translations, &action] {
                std::vector<std::string> names = gamepadNamesOf(controls, action.control);
                for (std::string &name : names) {
                    name = translations.text(name);
                }
                return names;
            },
        // While a key is chosen, only the rows it concerns stay bright
        .dimmed =
            [&action, capture] {
                return capture->action != nullptr && capture->action != &action && capture->owner != &action;
            },
        .width = tableWidth,
        .keysColumn = keysColumn,
        .buttonsColumn = buttonsColumn,
    };
    const auto choose = [&engine, &action, capture] {
        *capture = Capture{.action = &action, .resetButton = capture->resetButton};
        engine.getEcs().addComponent<ecs::Hidden>(capture->resetButton);
        engine.pushScene(KeyCaptureScene);
    };
    const std::string key = actionKey(action);
    const ecs::entity_t entity = interface.keyRow(translations.text(key), std::move(row), choose,
                                                  {engine::AnchorPoint::TopLeft, screenMargin, top});
    settings_page::bindText(engine.getEcs(), entity, translations, key);
}

/// The heads of the columns of the table, and the rule under them.
void addHeads(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
              const localization::Localization &translations)
{
    const std::vector<std::pair<std::string, float>> heads{
        {"controls.column.action", screenMargin + style.setting.inset},
        {"controls.column.keyboard", screenMargin + keysColumn},
        {"controls.column.gamepad", screenMargin + buttonsColumn},
    };
    for (const auto &[key, left] : heads) {
        const ecs::entity_t head =
            interface.sectionLabel(translations.text(key), {engine::AnchorPoint::TopLeft, left, headsTop});
        settings_page::bindText(ecs, head, translations, key);
    }
    interface.box(tableWidth, headsRuleThickness, headsRule,
                  {engine::AnchorPoint::TopLeft, screenMargin, headsRuleTop});
}

/// The name of a group of the table, without rule.
/// @return The top of its first row.
float addGroupName(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const engine::InterfaceStyle &style,
                   const localization::Localization &translations, const std::string &key, float top)
{
    const ecs::entity_t name = interface.sectionLabel(
        translations.text(key), {engine::AnchorPoint::TopLeft, screenMargin + style.setting.inset, top});
    settings_page::bindText(ecs, name, translations, key);
    return top + settings_page::labelHeight;
}

/// The top of the button of the default keys, under the table: the banner of a choice of key takes its place.
float bottomTop(const engine::InterfaceStyle &style)
{
    const float rows = static_cast<float>(GameActions.size()) * style.keys.rowHeight;
    return groupsTop + (2.0f * settings_page::labelHeight) + groupGap + rows + bottomGap;
}

void buildControlsPage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                       const localization::Localization &translations, const SharedCapture &capture,
                       const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    settings_page::addVeil(interface, ecs);
    interface.boundHeader([&translations] { return translations.text("settings.controls"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    addHeads(interface, ecs, style, translations);

    float top = addGroupName(interface, ecs, style, translations, "controls.group.movement", groupsTop);
    for (std::size_t index = 0; index < GameActions.size(); ++index) {
        if (index == fireGroupStart) {
            top = addGroupName(interface, ecs, style, translations, "controls.group.fire", top + groupGap);
        }
        addActionRow(interface, engine, translations, GameActions[index], capture, top);
        top += style.keys.rowHeight;
    }

    capture->resetButton = interface.button(translations.text("controls.reset"),
                                            [&engine] { engine.getControls().resetDefaults(); },
                                            {engine::AnchorPoint::TopLeft, screenMargin, bottomTop(style)});
    settings_page::bindText(ecs, capture->resetButton, translations, "controls.reset");
    settings_page::addKeys(interface, ecs, translations, "controls.keys");
}

/// The texts of the banner of the scene that asks for a key: while it waits for the key, and while it asks to
/// take the key from another action.
struct Banner {
    ecs::entity_t waitingTitle;
    ecs::entity_t waitingLine;
    ecs::entity_t takenTitle;
    ecs::entity_t takenLine;
};

/// Shows the texts of the banner of the key already taken, instead of those of the wait.
void showTaken(ecs::Ecs &ecs, const Banner &banner)
{
    for (const ecs::entity_t text : {banner.waitingTitle, banner.waitingLine}) {
        if (!ecs.hasComponent<ecs::Hidden>(text)) {
            ecs.addComponent<ecs::Hidden>(text);
        }
    }
    for (const ecs::entity_t text : {banner.takenTitle, banner.takenLine}) {
        if (ecs.hasComponent<ecs::Hidden>(text)) {
            ecs.removeComponent<ecs::Hidden>(text);
        }
    }
}

/// The end of the choice of a key, chosen or not: the page shows the keys of every action again, and its
/// button of the default keys.
void endCapture(engine::Engine &engine, Capture &capture)
{
    ecs::Ecs &ecs = engine.getEcs();
    if (ecs.isAlive(capture.resetButton) && ecs.hasComponent<ecs::Hidden>(capture.resetButton)) {
        ecs.removeComponent<ecs::Hidden>(capture.resetButton);
    }
    capture = Capture{.resetButton = capture.resetButton};
    engine.popScene();
}

void listenForKey(engine::Engine &engine, const SharedCapture &capture, const Banner &banner);

/// A key pressed for the action: it is bound at once if no other action has it. Otherwise the banner asks
/// first: Enter gives it to the action, another key is chosen instead, Escape keeps the keys as they were.
void onKey(engine::Engine &engine, const SharedCapture &capture, const Banner &banner, input::Key key)
{
    if (capture->action == nullptr) {
        return;
    }
    engine::Controls &controls = engine.getControls();
    const engine::Control control = capture->action->control;
    if (capture->key && key == input::Key::Enter) {
        assignKey(controls, control, *capture->key);
        endCapture(engine, *capture);
        return;
    }
    const GameAction *owner = findKeyOwner(controls, control, key);
    if (owner == nullptr) {
        assignKey(controls, control, key);
        endCapture(engine, *capture);
        return;
    }
    capture->key = key;
    capture->owner = owner;
    showTaken(engine.getEcs(), banner);
    listenForKey(engine, capture, banner);
}

/// Waits for the next key, on the title of the banner.
void listenForKey(engine::Engine &engine, const SharedCapture &capture, const Banner &banner)
{
    engine.getEcs().addComponent<KeyCapture>(banner.waitingTitle,
                                             KeyCapture{.onKey = [&engine, capture, banner](input::Key key) {
                                                 onKey(engine, capture, banner, key);
                                             }});
}

/// A title of the banner, in the font of the values, written from a function.
ecs::entity_t addBannerTitle(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                             const engine::InterfaceStyle &style, std::function<std::string()> get,
                             render::Color color, float top)
{
    const ecs::entity_t title =
        interface.note(get(), {engine::AnchorPoint::TopLeft, screenMargin + style.setting.inset, top});
    engine::Text &text = ecs.getComponent<engine::Text>(title);
    text.font = style.setting.valueFont;
    text.size = bannerTitleSize;
    text.color = color;
    text.spacing = bannerTitleSpacing;
    ecs.addComponent<engine::BoundText>(title, std::move(get));
    return title;
}

/// A line of the banner, under its title, written from a function.
ecs::entity_t addBannerLine(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                            const engine::InterfaceStyle &style, std::function<std::string()> get, float top)
{
    const ecs::entity_t line =
        interface.note(get(), {engine::AnchorPoint::TopLeft, screenMargin + style.setting.inset, top});
    ecs.addComponent<engine::BoundText>(line, std::move(get));
    return line;
}

/// "New key for: Fire" under the table, over the page, which stays visible: the row of the action shows the
/// key waited for, the others dim. The first key pressed goes to the action, unless another one has it.
void buildKeyCapture(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                     const localization::Localization &translations, const SharedCapture &capture)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    const float top = bottomTop(style);
    const auto actionName = [&translations, capture] {
        return capture->action != nullptr ? translations.text(actionKey(*capture->action)) : std::string();
    };
    const auto takenTitle = [&translations, capture] {
        if (!capture->key || capture->owner == nullptr) {
            return std::string();
        }
        return keyText(translations, *capture->key) + " " + translations.text("controls.used_by") + " " +
               translations.text(actionKey(*capture->owner));
    };
    const Banner banner{
        .waitingTitle = addBannerTitle(
            interface, ecs, style,
            [&translations, actionName] { return translations.text("controls.press") + " " + actionName(); },
            style.setting.litValue, top),
        .waitingLine = addBannerLine(
            interface, ecs, style, [&translations] { return translations.text("controls.cancel"); },
            top + bannerLineGap),
        .takenTitle = addBannerTitle(interface, ecs, style, takenTitle, style.keys.conflict, top),
        .takenLine = addBannerLine(
            interface, ecs, style,
            [&translations, actionName] {
                return translations.text("controls.give") + " " + actionName() + "      " +
                       translations.text("controls.keep");
            },
            top + bannerLineGap),
    };
    ecs.addComponent<ecs::Hidden>(banner.takenTitle);
    ecs.addComponent<ecs::Hidden>(banner.takenLine);
    listenForKey(engine, capture, banner);
}

} // namespace

void createControlsScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                          localization::Localization &translations)
{
    const auto capture = std::make_shared<Capture>();
    // Written when the page closes, not at each key
    const auto close = [&engine] {
        keepControlSettings(engine.getSettings(), engine.getControls());
        writeSettings(engine.getSettings());
        engine.popScene();
    };
    engine.createScene(
        ControlsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
        [&style, &translations, capture, close](engine::Engine &target, engine::SceneId scene) {
            buildControlsPage(target, scene, style, translations, capture, close);
        });
    // Over the page, which stays visible; Escape keeps the keys as they were
    engine.createScene(KeyCaptureScene,
                       {.overlay = true, .onBack = [&engine, capture] { endCapture(engine, *capture); }},
                       [&style, &translations, capture](engine::Engine &target, engine::SceneId scene) {
                           buildKeyCapture(target, scene, style, translations, capture);
                       });
}

} // namespace rtype::client
