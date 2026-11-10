#include "Controls/KeyChoice.hpp"

#include "Controls/KeyCaptureSystem.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/input/Devices.hpp"

#include <functional>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// The banner of a choice of key: its title, and the line under it.
constexpr float bannerTitleSize = 26.0f;
constexpr float bannerTitleSpacing = 5.0f;
constexpr float bannerLineGap = 44.0f;

/// The keys of a row: those of its action, the key waited for or chosen while the player chooses one for it,
/// and the key that would leave it in red.
std::vector<engine::KeyCap> keysOfRow(const engine::Controls &controls,
                                      const localization::Localization &translations,
                                      const GameAction &action, const KeyChoice &choice)
{
    if (choice.action == &action) {
        if (choice.key) {
            return {{keyText(translations, *choice.key), engine::KeyCapState::New}};
        }
        return {{"", engine::KeyCapState::Waiting}};
    }
    std::vector<engine::KeyCap> keys;
    for (const input::Key key : keysOf(controls, action.control)) {
        const bool taken = choice.owner == &action && choice.key == key;
        keys.push_back({keyText(translations, key),
                        taken ? engine::KeyCapState::Conflict : engine::KeyCapState::Normal});
    }
    if (keys.empty()) {
        keys.push_back({translations.text("controls.none"), engine::KeyCapState::Normal});
    }
    return keys;
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

/// The end of the choice of a key, chosen or not: the page shows the keys of every action again, and what
/// the banner hid.
void endChoice(engine::Engine &engine, KeyChoice &choice)
{
    ecs::Ecs &ecs = engine.getEcs();
    if (choice.hidden && ecs.isAlive(*choice.hidden) && ecs.hasComponent<ecs::Hidden>(*choice.hidden)) {
        ecs.removeComponent<ecs::Hidden>(*choice.hidden);
    }
    choice = KeyChoice{.hidden = choice.hidden};
    engine.popScene();
}

void listenForKey(engine::Engine &engine, const SharedKeyChoice &choice, const Banner &banner);

/// A key pressed for the action: it is bound at once if no other action has it. Otherwise the banner asks
/// first: Enter gives it to the action, another key is chosen instead, Escape keeps the keys as they were.
void onKey(engine::Engine &engine, const SharedKeyChoice &choice, const Banner &banner, input::Key key)
{
    if (choice->action == nullptr) {
        return;
    }
    engine::Controls &controls = engine.getControls();
    const engine::Control control = choice->action->control;
    if (choice->key && key == input::Key::Enter) {
        assignKey(controls, control, *choice->key);
        endChoice(engine, *choice);
        return;
    }
    const GameAction *owner = findKeyOwner(controls, control, key);
    if (owner == nullptr) {
        assignKey(controls, control, key);
        endChoice(engine, *choice);
        return;
    }
    choice->key = key;
    choice->owner = owner;
    showTaken(engine.getEcs(), banner);
    listenForKey(engine, choice, banner);
}

/// Waits for the next key, on the title of the banner.
void listenForKey(engine::Engine &engine, const SharedKeyChoice &choice, const Banner &banner)
{
    engine.getEcs().addComponent<KeyCapture>(banner.waitingTitle,
                                             KeyCapture{.onKey = [&engine, choice, banner](input::Key key) {
                                                 onKey(engine, choice, banner, key);
                                             }});
}

/// A title of the banner, in the font of the values, written from a function.
ecs::entity_t addBannerTitle(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                             const engine::InterfaceStyle &style, std::function<std::string()> get,
                             render::Color color, float left, float top)
{
    const ecs::entity_t title = interface.note(get(), {engine::AnchorPoint::TopLeft, left, top});
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
                            std::function<std::string()> get, float left, float top)
{
    const ecs::entity_t line = interface.note(get(), {engine::AnchorPoint::TopLeft, left, top});
    ecs.addComponent<engine::BoundText>(line, std::move(get));
    return line;
}

/// "New key for: Fire", over the page, which stays visible: the row of the action shows the key waited for,
/// the others dim. The first key pressed goes to the action, unless another one has it.
void buildKeyChoice(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                    const localization::Localization &translations, const SharedKeyChoice &choice, float left,
                    float top)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    left += style.setting.inset;
    const auto actionName = [&translations, choice] {
        return choice->action != nullptr ? translations.text(actionTextKey(*choice->action)) : std::string();
    };
    const auto takenTitle = [&translations, choice] {
        if (!choice->key || choice->owner == nullptr) {
            return std::string();
        }
        return keyText(translations, *choice->key) + " " + translations.text("controls.used_by") + " " +
               translations.text(actionTextKey(*choice->owner));
    };
    const Banner banner{
        .waitingTitle = addBannerTitle(
            interface, ecs, style,
            [&translations, actionName] { return translations.text("controls.press") + " " + actionName(); },
            style.setting.litValue, left, top),
        .waitingLine = addBannerLine(
            interface, ecs, [&translations] { return translations.text("controls.cancel"); }, left,
            top + bannerLineGap),
        .takenTitle = addBannerTitle(interface, ecs, style, takenTitle, style.keys.conflict, left, top),
        .takenLine = addBannerLine(
            interface, ecs,
            [&translations, actionName] {
                return translations.text("controls.give") + " " + actionName() + "      " +
                       translations.text("controls.keep");
            },
            left, top + bannerLineGap),
    };
    ecs.addComponent<ecs::Hidden>(banner.takenTitle);
    ecs.addComponent<ecs::Hidden>(banner.takenLine);
    listenForKey(engine, choice, banner);
}

} // namespace

std::string actionTextKey(const GameAction &action)
{
    return std::string(ControlsSettingsPrefix).append(action.name);
}

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

ecs::entity_t addActionRow(engine::InterfaceBuilder &interface, engine::Engine &engine,
                           const localization::Localization &translations, const GameAction &action,
                           const SharedKeyChoice &choice, const KeyTable &table, const char *chooseScene,
                           float top)
{
    const engine::Controls &controls = engine.getControls();
    engine::KeyRow row{
        .keys = [&controls, &translations, &action,
                 choice] { return keysOfRow(controls, translations, action, *choice); },
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
            [&action, choice] {
                return choice->action != nullptr && choice->action != &action && choice->owner != &action;
            },
        .width = table.width,
        .keysColumn = table.keysColumn,
        .buttonsColumn = table.buttonsColumn,
    };
    const auto choose = [&engine, &action, choice, chooseScene] {
        *choice = KeyChoice{.action = &action, .hidden = choice->hidden};
        if (choice->hidden) {
            engine.getEcs().addComponent<ecs::Hidden>(*choice->hidden);
        }
        engine.pushScene(chooseScene);
    };
    const std::string key = actionTextKey(action);
    const ecs::entity_t entity = interface.keyRow(translations.text(key), std::move(row), choose,
                                                  {engine::AnchorPoint::TopLeft, table.left, top});
    settings_page::bindText(engine.getEcs(), entity, translations, key);
    return entity;
}

void createKeyChoiceScene(engine::Engine &engine, const char *name, const engine::InterfaceStyle &style,
                          const localization::Localization &translations, const SharedKeyChoice &choice,
                          float left, float top)
{
    // Over the page, which stays visible; Escape keeps the keys as they were
    engine.createScene(
        name, {.overlay = true, .onBack = [&engine, choice] { endChoice(engine, *choice); }},
        [&style, &translations, choice, left, top](engine::Engine &target, engine::SceneId scene) {
            buildKeyChoice(target, scene, style, translations, choice, left, top);
        });
}

} // namespace rtype::client
