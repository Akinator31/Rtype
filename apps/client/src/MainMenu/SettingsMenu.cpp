#include "MainMenu/SettingsMenu.hpp"

#include "MainMenu/ControlsMenu.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/DisplaySettings.hpp"
#include "Settings/NextValue.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/engine/Component/Widget.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// How much a key or a notch of the wheel changes a volume.
constexpr float volumeStep = 0.05f;

/// The sliders of the page, with the key of their caption: their caption is plain text, written again when
/// the language changes.
using SliderCaptions = std::vector<std::pair<ecs::entity_t, std::string>>;

/// What every part of the page needs to be built.
struct Page {
    engine::Engine &engine;
    ecs::Ecs &ecs;
    engine::InterfaceBuilder &interface;
    const engine::InterfaceStyle &style;
    const SettingsTargets &targets;
    /// The hint of each row, shown while it has the focus.
    std::shared_ptr<settings_page::Hints> hints;
};

/// A row that goes through values, its caption following the language, with its hint.
/// @return The top of the next row.
float addRow(Page &page, const std::string &captionKey, std::function<std::string()> value,
             std::function<void(int)> change, const std::string &hintKey, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    const ecs::entity_t row =
        page.interface.settingRow(translations.text(captionKey), std::move(value), std::move(change),
                                  {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(page.ecs, row, translations, captionKey);
    page.hints->emplace_back(row, hintKey);
    return top + page.style.setting.height;
}

/// "Language: French": English or French, and the captions of the sliders written again.
float addLanguageRow(Page &page, std::shared_ptr<const SliderCaptions> sliders, float left, float top)
{
    localization::Localization &translations = page.targets.translations;
    ecs::Ecs &ecs = page.ecs;
    const auto value = [&translations] {
        return translations.text(translations.language() == "fr" ? "language.french" : "language.english");
    };
    const auto change = [&ecs, &translations, sliders = std::move(sliders)](int /*direction*/) {
        translations.setLanguage(translations.language() == "fr" ? "en" : "fr");
        for (const auto &[slider, key] : *sliders) {
            ecs.getComponent<engine::Slider>(slider).caption = translations.text(key);
        }
    };
    return addRow(page, "settings.language", value, change, "settings.hint.language", left, top);
}

/// A slider of a volume, with the key of its caption.
std::pair<ecs::entity_t, std::string> addVolumeSlider(Page &page, const std::string &key,
                                                      std::function<float()> get,
                                                      std::function<void(float)> set, float left, float top)
{
    const ecs::entity_t slider =
        page.interface.slider(page.targets.translations.text(key), std::move(get), std::move(set), volumeStep,
                              {engine::AnchorPoint::TopLeft, left, top});
    page.hints->emplace_back(slider, "settings.hint.volume");
    return {slider, key};
}

/// The sliders of the music and of the sound effects, one under the other.
SliderCaptions addVolumeSliders(Page &page, float left, float top)
{
    audio::AudioPlayer &audio = page.targets.audio;
    const float height = page.style.slider.getHeight();
    return {addVolumeSlider(
                page, "settings.music_volume", [&audio] { return audio.musicVolume(); },
                [&audio](float volume) { audio.setMusicVolume(volume); }, left, top),
            addVolumeSlider(
                page, "settings.effects_volume", [&audio] { return audio.effectsVolume(); },
                [&audio](float volume) { audio.setEffectsVolume(volume); }, left, top + height)};
}

/// "Colour filter: Deuteranopia": goes through the filters of PlayerColorFilters.
float addColorFilterRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    render::Renderer &renderer = page.targets.renderer;
    const auto value = [&translations, &renderer] {
        const std::string_view name = colorFilterSettingName(renderer.colorFilter());
        return translations.text("settings.filter." + std::string(name.empty() ? "none" : name));
    };
    const auto change = [&renderer](int direction) {
        renderer.setColorFilter(stepValue(PlayerColorFilters, renderer.colorFilter(), direction));
    };
    return addRow(page, "settings.color_filter", value, change, "settings.hint.color_filter", left, top);
}

/// "Activity on Discord: On": whether the profile of the player in Discord shows what it plays.
float addDiscordActivityRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    PresenceTracker &presence = page.targets.presence;
    const auto value = [&translations, &presence] {
        return translations.text(presence.isEnabled() ? "settings.on" : "settings.off");
    };
    const auto change = [&presence](int /*direction*/) { presence.setEnabled(!presence.isEnabled()); };
    return addRow(page, "settings.discord_activity", value, change, "settings.hint.discord_activity", left,
                  top);
}

/// "Screen: 2 · DELL U2720Q": moves the game to another screen.
float addMonitorRow(Page &page, float left, float top)
{
    render::Window &window = page.targets.window;
    const auto value = [&window] {
        const std::vector<render::Monitor> monitors = window.monitors();
        const auto index = static_cast<std::size_t>(window.monitor());
        const std::string name = index < monitors.size() ? monitors[index].name : "";
        return std::to_string(index + 1) + (name.empty() ? "" : " · " + name);
    };
    const auto change = [&window](int direction) { moveToMonitor(window, direction); };
    return addRow(page, "settings.monitor", value, change, "settings.hint.monitor", left, top);
}

/// "Resolution: 1600 x 900": fullscreen and the sizes that fit on the screen of the window.
float addResolutionRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    render::Window &window = page.targets.window;
    const auto value = [&translations, &window] {
        const Resolution resolution = currentResolution(window);
        return resolution.isFullscreen()
                   ? translations.text("settings.fullscreen")
                   : std::to_string(resolution.width) + " x " + std::to_string(resolution.height);
    };
    const auto change = [&window](int direction) {
        setResolution(window, stepValue(resolutionsOf(window), currentResolution(window), direction));
    };
    return addRow(page, "settings.resolution", value, change, "settings.hint.resolution", left, top);
}

/// "Frame rate: 144": goes through FpsLimits. Above the refresh rate of the screen, it needs the vertical
/// synchronisation off.
float addFpsRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    render::Window &window = page.targets.window;
    const auto value = [&translations, &window] {
        const int limit = window.targetFps();
        return limit == UnlimitedFps ? translations.text("settings.unlimited") : std::to_string(limit);
    };
    const auto change = [&window](int direction) {
        window.setTargetFps(stepValue(FpsLimits, window.targetFps(), direction));
    };
    return addRow(page, "settings.fps_limit", value, change, "settings.hint.fps_limit", left, top);
}

/// "VSync: On": switches the vertical synchronisation on and off.
float addVsyncRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    render::Window &window = page.targets.window;
    const auto value = [&translations, &window] {
        return translations.text(window.isVsync() ? "settings.on" : "settings.off");
    };
    const auto change = [&window](int /*direction*/) { window.setVsync(!window.isVsync()); };
    return addRow(page, "settings.vsync", value, change, "settings.hint.vsync", left, top);
}

/// "Keyboard and gamepad": opens the Controls page.
void addControlsRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    engine::Engine &engine = page.engine;
    const ecs::entity_t row = page.interface.linkRow(translations.text("settings.keyboard_gamepad"),
                                                     [&engine] { engine.pushScene(ControlsMenuScene); },
                                                     {engine::AnchorPoint::TopLeft, left, top});
    settings_page::bindText(page.ecs, row, translations, "settings.keyboard_gamepad");
    page.hints->emplace_back(row, "settings.hint.controls");
}

/// On the left, the language, the volumes, the colour filter and the activity on Discord; on the right, the
/// display and the controls. The widgets get the focus in the order they are made: down the left column, then
/// down the right one.
void buildSettingsPage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                       const SettingsTargets &targets, const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    const localization::Localization &translations = targets.translations;
    engine::InterfaceBuilder interface(ecs, scene, style);
    settings_page::addVeil(interface, ecs);
    interface.boundHeader([&translations] { return translations.text("menu.settings"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    Page page{engine, ecs, interface, style, targets, std::make_shared<settings_page::Hints>()};

    const float width = style.setting.width;
    const float left = screenMargin;
    const float right = screenMargin + width + settings_page::columnGap;
    using settings_page::addGroup;
    using settings_page::groupGap;

    // The sliders are known after the language row, which writes their captions again
    const auto sliders = std::make_shared<SliderCaptions>();
    float top = addGroup(interface, ecs, style, translations, "settings.group.general", left,
                         settings_page::top, width);
    top = addLanguageRow(page, sliders, left, top) + groupGap;
    top = addGroup(interface, ecs, style, translations, "settings.group.audio", left, top, width);
    *sliders = addVolumeSliders(page, left, top);
    top += 2.0f * style.slider.getHeight() + groupGap;
    top = addGroup(interface, ecs, style, translations, "settings.group.accessibility", left, top, width);
    top = addColorFilterRow(page, left, top) + groupGap;
    top = addGroup(interface, ecs, style, translations, "settings.group.discord", left, top, width);
    addDiscordActivityRow(page, left, top);

    top = addGroup(interface, ecs, style, translations, "settings.group.display", right, settings_page::top,
                   width);
    top = addMonitorRow(page, right, top);
    top = addResolutionRow(page, right, top);
    top = addFpsRow(page, right, top);
    top = addVsyncRow(page, right, top) + groupGap;
    top = addGroup(interface, ecs, style, translations, "settings.group.controls", right, top, width);
    addControlsRow(page, right, top);

    // The hint of the focused row, under the right column
    top += style.setting.height + groupGap;
    settings_page::addHint(interface, ecs, translations, page.hints,
                           {engine::AnchorPoint::TopLeft, right + style.setting.inset, top});
    settings_page::addKeys(interface, ecs, translations, "settings.keys");
}

} // namespace

void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style, SettingsTargets targets)
{
    // Written when the page closes, not at every step of a slider
    const auto close = [&engine, targets] {
        engine::Settings &settings = engine.getSettings();
        keepPlayerSettings(settings, targets.translations, targets.audio, targets.renderer.colorFilter());
        keepDisplaySettings(settings, targets.window);
        settings.setBool(DiscordActivitySettingsKey, targets.presence.isEnabled());
        writeSettings(settings);
        engine.popScene();
    };
    engine.createScene(SettingsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, targets, close](engine::Engine &target, engine::SceneId scene) {
                           buildSettingsPage(target, scene, style, targets, close);
                       });
}

} // namespace rtype::client
