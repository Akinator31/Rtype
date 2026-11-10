#include "MainMenu/VrMenu.hpp"

#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/NextValue.hpp"
#include "Settings/PlayerSettings.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace rtype::client
{

std::string metresText(float metres)
{
    // One decimal, two when it is needed: 2.45 m
    const long hundredths = std::lround(metres * 100.0f);
    return hundredths % 10 == 0 ? std::format("{:.1f} m", static_cast<float>(hundredths) / 100.0f)
                                : std::format("{:.2f} m", static_cast<float>(hundredths) / 100.0f);
}

std::string centimetresText(float metres)
{
    const long centimetres = std::lround(metres * 100.0f);
    return centimetres > 0 ? std::format("+{} cm", centimetres) : std::format("{} cm", centimetres);
}

namespace
{

/// What every part of the page needs to be built.
struct Page {
    engine::Engine &engine;
    ecs::Ecs &ecs;
    engine::InterfaceBuilder &interface;
    const engine::InterfaceStyle &style;
    const localization::Localization &translations;
    const VrTargets &targets;
    /// The hint of each row, shown while it has the focus.
    std::shared_ptr<settings_page::Hints> hints;
};

/// A row that goes through values, its caption following the language, with its hint.
/// @return The top of the next row.
float addRow(Page &page, const std::string &captionKey, std::function<std::string()> value,
             std::function<void(int)> change, const std::string &hintKey, float left, float top)
{
    const ecs::entity_t row =
        page.interface.settingRow(page.translations.text(captionKey), std::move(value), std::move(change),
                                  {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(page.ecs, row, page.translations, captionKey);
    page.hints->emplace_back(row, hintKey);
    return top + page.style.setting.height;
}

/// "VR: On": opens the headset, or closes it, at once; without a headset, a toast says so and the client
/// stays on the screen, the choice kept for the next start.
float addEnabledRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.translations;
    const VrTargets &targets = page.targets;
    engine::Engine &engine = page.engine;
    const auto value = [&translations, &targets] {
        return translations.text(targets.preferences.enabled ? "settings.on" : "settings.off");
    };
    const auto change = [&translations, &targets, &engine](int /*direction*/) {
        VrPreferences &preferences = targets.preferences;
        preferences.enabled = !preferences.enabled;
        if (!targets.switchHeadset(preferences.enabled) && preferences.enabled) {
            engine.showToast(translations.text("vr.toast.no_headset"), engine::ToastType::Warning);
        }
    };
    return addRow(page, "vr.enabled", value, change, "vr.hint.enabled", left, top);
}

/// "Recentre": puts the cockpit back around the player.
float addRecenterRow(Page &page, float left, float top)
{
    const VrTargets &targets = page.targets;
    const ecs::entity_t row =
        page.interface.linkRow(page.translations.text("vr.recenter"), [&targets] { targets.recenter(); },
                               {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(page.ecs, row, page.translations, "vr.recenter");
    page.hints->emplace_back(row, "vr.hint.recenter");
    return top + page.style.setting.height;
}

/// "Snap turn: 45°": goes through SnapTurns, Off first.
float addSnapTurnRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.translations;
    VrPreferences &preferences = page.targets.preferences;
    const auto value = [&translations, &preferences] {
        return preferences.snapTurn == 0 ? translations.text("settings.off")
                                         : std::format("{}°", preferences.snapTurn);
    };
    const auto change = [&preferences](int direction) {
        preferences.snapTurn = stepValue(SnapTurns, preferences.snapTurn, direction);
    };
    return addRow(page, "vr.snap_turn", value, change, "vr.hint.snap_turn", left, top);
}

/// "Window: Left eye": what the window shows while the headset shows the game.
float addMirrorRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.translations;
    VrPreferences &preferences = page.targets.preferences;
    const auto value = [&translations, &preferences] {
        return translations.text("vr.mirror." + std::string(mirrorName(preferences.mirror)));
    };
    const auto change = [&preferences](int direction) {
        preferences.mirror = stepValue(VrMirrors, preferences.mirror, direction);
    };
    return addRow(page, "vr.mirror", value, change, "vr.hint.mirror", left, top);
}

/// "Board: In relief": in relief for each eye, or flat.
float addReliefRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.translations;
    VrPreferences &preferences = page.targets.preferences;
    const auto value = [&translations, &preferences] {
        return translations.text(preferences.flatBoard ? "vr.board.flat" : "vr.board.relief");
    };
    const auto change = [&preferences](int /*direction*/) { preferences.flatBoard = !preferences.flatBoard; };
    return addRow(page, "vr.board", value, change, "vr.hint.board", left, top);
}

/// A row of a length in metres that goes through the values offered.
template <std::size_t Count>
float addLengthRow(Page &page, const std::string &key, float &length, const std::array<float, Count> &offered,
                   const std::function<std::string(float)> &text, float left, float top)
{
    const auto value = [&length, text] { return text(length); };
    const auto change = [&length, &offered](int direction) {
        length = stepValue(offered, length, direction);
    };
    return addRow(page, "vr." + key, value, change, "vr.hint." + key, left, top);
}

/// On the left, the headset; on the right, the board and the seat. The widgets get the focus in the order
/// they are made: down the left column, then down the right one.
void buildVrPage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                 const localization::Localization &translations, const VrTargets &targets,
                 const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    engine::InterfaceBuilder interface(ecs, scene, style);
    settings_page::addVeil(interface, ecs);
    interface.boundHeader([&translations] { return translations.text("vr.title"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    Page page{engine, ecs, interface, style, translations, targets, std::make_shared<settings_page::Hints>()};

    const float width = style.setting.width;
    const float left = screenMargin;
    const float right = screenMargin + width + settings_page::columnGap;
    using settings_page::addGroup;
    VrPreferences &preferences = targets.preferences;

    float top =
        addGroup(interface, ecs, style, translations, "vr.group.headset", left, settings_page::top, width);
    top = addEnabledRow(page, left, top);
    top = addRecenterRow(page, left, top);
    top = addSnapTurnRow(page, left, top);
    addMirrorRow(page, left, top);

    top = addGroup(interface, ecs, style, translations, "vr.group.board", right, settings_page::top, width);
    top = addReliefRow(page, right, top);
    top = addLengthRow(page, "board_distance", preferences.boardDistance, BoardDistances, metresText, right,
                       top);
    top = addLengthRow(page, "board_width", preferences.boardWidth, BoardWidths, metresText, right, top);
    top = addGroup(interface, ecs, style, translations, "vr.group.seat", right, top + settings_page::groupGap,
                   width);
    top = addLengthRow(page, "seat_height", preferences.seatHeight, SeatHeights, centimetresText, right, top);

    // The hint of the focused row, under the right column
    settings_page::addHint(
        interface, ecs, translations, page.hints,
        {engine::AnchorPoint::TopLeft, right + style.setting.inset, top + settings_page::groupGap});
    settings_page::addKeys(interface, ecs, translations, "settings.keys");
}

} // namespace

void createVrScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                   const localization::Localization &translations, const VrTargets &targets)
{
    // Written when the page closes, not at every step
    const auto close = [&engine, &targets] {
        engine::Settings &settings = engine.getSettings();
        keepVrSettings(settings, targets.preferences);
        writeSettings(settings);
        engine.popScene();
    };
    engine.createScene(
        VrMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
        [&style, &translations, &targets, close](engine::Engine &target, engine::SceneId scene) {
            buildVrPage(target, scene, style, translations, targets, close);
        });
}

} // namespace rtype::client
