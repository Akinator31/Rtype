#include "MainMenu/ControlsMenu.hpp"

#include "Controls/KeyChoice.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/ControlSettings.hpp"
#include "Settings/PlayerSettings.hpp"

#include <cstddef>
#include <functional>
#include <memory>
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
/// The table of the page.
constexpr KeyTable table{
    .left = screenMargin, .width = tableWidth, .keysColumn = keysColumn, .buttonsColumn = buttonsColumn};

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
                       const localization::Localization &translations, const SharedKeyChoice &choice,
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
        addActionRow(interface, engine, translations, GameActions[index], choice, table, KeyCaptureScene,
                     top);
        top += style.keys.rowHeight;
    }

    const ecs::entity_t reset = interface.button(
        translations.text("controls.reset"), [&engine] { engine.getControls().resetDefaults(); },
        {engine::AnchorPoint::TopLeft, screenMargin, bottomTop(style)});
    choice->hidden = reset;
    settings_page::bindText(ecs, reset, translations, "controls.reset");
}

} // namespace

void createControlsScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                          localization::Localization &translations)
{
    const auto choice = std::make_shared<KeyChoice>();
    // Written when the page closes, not at each key
    const auto close = [&engine] {
        keepControlSettings(engine.getSettings(), engine.getControls());
        writeSettings(engine.getSettings());
        engine.popScene();
    };
    engine.createScene(ControlsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, &translations, choice, close](engine::Engine &target, engine::SceneId scene) {
                           buildControlsPage(target, scene, style, translations, choice, close);
                       });
    // The banner takes the place of the button of the default keys
    createKeyChoiceScene(engine, KeyCaptureScene, style, translations, choice, screenMargin,
                         bottomTop(style));
}

} // namespace rtype::client
