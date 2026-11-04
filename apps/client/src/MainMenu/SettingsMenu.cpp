#include "MainMenu/SettingsMenu.hpp"

#include "MainMenu/Column.hpp"
#include "Settings/DisplaySettings.hpp"
#include "Settings/NextValue.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/engine/Component/Widget.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// The rows of the page, under its header: the language, the two volumes, the colour filter, the screen, the
/// resolution, the limit of frames per second, then the vertical synchronisation.
enum Row : int { LanguageRow, MusicRow, EffectsRow, FilterRow, MonitorRow, ResolutionRow, FpsRow, VsyncRow };
constexpr float rowSpacing = 88.0f;
/// How much a key or a notch of the wheel changes a volume.
constexpr float volumeStep = 0.05f;

float rowTop(Row row)
{
    return columnTop + (static_cast<float>(row) * rowSpacing);
}

/// "Caption: value", the text of a button that goes through choices.
std::string choiceLabel(const localization::Localization &translations, const std::string &captionKey,
                        const std::string &value)
{
    return translations.text(captionKey) + ": " + value;
}

/// The sliders of the page, with the key of their caption: their caption is plain text, written again when
/// the language changes.
using SliderCaptions = std::vector<std::pair<ecs::entity_t, std::string>>;

/// "Language: French": switches between English and French, and writes the captions of the sliders again.
void addLanguageButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                       localization::Localization &translations,
                       std::shared_ptr<const SliderCaptions> sliders)
{
    const auto label = [&translations] {
        const bool french = translations.language() == "fr";
        return choiceLabel(translations, "settings.language",
                           translations.text(french ? "language.french" : "language.english"));
    };
    const auto next = [&ecs, &translations, sliders = std::move(sliders)] {
        translations.setLanguage(translations.language() == "fr" ? "en" : "fr");
        for (const auto &[slider, key] : *sliders) {
            ecs.getComponent<engine::Slider>(slider).caption = translations.text(key);
        }
    };
    boundButton(interface, ecs, label, next, rowTop(LanguageRow));
}

/// A slider of a volume, with the key of its caption.
std::pair<ecs::entity_t, std::string> volumeSlider(engine::InterfaceBuilder &interface,
                                                   const engine::InterfaceStyle &style,
                                                   const localization::Localization &translations,
                                                   const std::string &key, std::function<float()> get,
                                                   std::function<void(float)> set, Row row)
{
    // The area of a slider goes beyond its rail by the handle: the rail starts at the margin, like the
    // buttons
    const float left = screenMargin - style.slider.handleDragRadius;
    return {interface.slider(translations.text(key), std::move(get), std::move(set), volumeStep,
                             {engine::AnchorPoint::TopLeft, left, rowTop(row)}),
            key};
}

/// The sliders of the music and of the sound effects.
SliderCaptions addVolumeSliders(engine::InterfaceBuilder &interface, const engine::InterfaceStyle &style,
                                const SettingsTargets &targets)
{
    audio::AudioPlayer &audio = targets.audio;
    return {volumeSlider(
                interface, style, targets.translations, "settings.music_volume",
                [&audio] { return audio.musicVolume(); },
                [&audio](float volume) { audio.setMusicVolume(volume); }, MusicRow),
            volumeSlider(
                interface, style, targets.translations, "settings.effects_volume",
                [&audio] { return audio.effectsVolume(); },
                [&audio](float volume) { audio.setEffectsVolume(volume); }, EffectsRow)};
}

/// "Colour filter: Deuteranopia": goes through the filters of PlayerColorFilters.
void addColorFilterButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    const localization::Localization &translations = targets.translations;
    render::Renderer &renderer = targets.renderer;
    const auto label = [&translations, &renderer] {
        const std::string_view name = colorFilterSettingName(renderer.colorFilter());
        return choiceLabel(translations, "settings.color_filter",
                           translations.text("settings.filter." + std::string(name.empty() ? "none" : name)));
    };
    const auto next = [&renderer] {
        renderer.setColorFilter(nextValue(PlayerColorFilters, renderer.colorFilter()));
    };
    boundButton(interface, ecs, label, next, rowTop(FilterRow));
}

/// "Screen: 2 (DELL U2720Q)": moves the game to the next screen.
void addMonitorButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    const localization::Localization &translations = targets.translations;
    render::Window &window = targets.window;
    const auto label = [&translations, &window] {
        const std::vector<render::Monitor> monitors = window.monitors();
        const auto index = static_cast<std::size_t>(window.monitor());
        const std::string name = index < monitors.size() ? monitors[index].name : "";
        return choiceLabel(translations, "settings.monitor",
                           std::to_string(index + 1) + (name.empty() ? "" : " (" + name + ")"));
    };
    boundButton(interface, ecs, label, [&window] { moveToNextMonitor(window); }, rowTop(MonitorRow));
}

/// "Resolution: 1600 x 900": goes through fullscreen and the sizes that fit on the screen of the window.
void addResolutionButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    const localization::Localization &translations = targets.translations;
    render::Window &window = targets.window;
    const auto label = [&translations, &window] {
        const Resolution resolution = currentResolution(window);
        return choiceLabel(translations, "settings.resolution",
                           resolution.isFullscreen() ? translations.text("settings.fullscreen")
                                                     : std::to_string(resolution.width) + " x " +
                                                           std::to_string(resolution.height));
    };
    const auto next = [&window] {
        setResolution(window, nextValue(resolutionsOf(window), currentResolution(window)));
    };
    boundButton(interface, ecs, label, next, rowTop(ResolutionRow));
}

/// "Frame rate: 144": goes through FpsLimits. Above the refresh rate of the screen, it needs the vertical
/// synchronisation off.
void addFpsButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    const localization::Localization &translations = targets.translations;
    render::Window &window = targets.window;
    const auto label = [&translations, &window] {
        const int limit = window.targetFps();
        return choiceLabel(translations, "settings.fps_limit",
                           limit == UnlimitedFps ? translations.text("settings.unlimited")
                                                 : std::to_string(limit));
    };
    const auto next = [&window] { window.setTargetFps(nextValue(FpsLimits, window.targetFps())); };
    boundButton(interface, ecs, label, next, rowTop(FpsRow));
}

/// "VSync: On": switches the vertical synchronisation on and off.
void addVsyncButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    const localization::Localization &translations = targets.translations;
    render::Window &window = targets.window;
    const auto label = [&translations, &window] {
        return choiceLabel(translations, "settings.vsync",
                           translations.text(window.isVsync() ? "settings.on" : "settings.off"));
    };
    boundButton(interface, ecs, label, [&window] { window.setVsync(!window.isVsync()); }, rowTop(VsyncRow));
}

void buildSettingsPage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                       const SettingsTargets &targets, const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    localization::Localization &translations = targets.translations;
    engine::InterfaceBuilder interface(ecs, scene, style);
    interface.boundHeader([&translations] { return translations.text("menu.settings"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    // The widgets get the focus in the order they are made: the sliders are known after the language
    const auto sliders = std::make_shared<SliderCaptions>();
    addLanguageButton(interface, ecs, translations, sliders);
    *sliders = addVolumeSliders(interface, style, targets);
    addColorFilterButton(interface, ecs, targets);
    addMonitorButton(interface, ecs, targets);
    addResolutionButton(interface, ecs, targets);
    addFpsButton(interface, ecs, targets);
    addVsyncButton(interface, ecs, targets);
}

} // namespace

void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style, SettingsTargets targets)
{
    // Written when the page closes, not at every step of a slider
    const auto close = [&engine, targets] {
        engine::Settings &settings = engine.getSettings();
        keepPlayerSettings(settings, targets.translations, targets.audio, targets.renderer.colorFilter());
        keepDisplaySettings(settings, targets.window);
        writeSettings(settings);
        engine.popScene();
    };
    engine.createScene(SettingsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, targets, close](engine::Engine &target, engine::SceneId scene) {
                           buildSettingsPage(target, scene, style, targets, close);
                       });
}

} // namespace rtype::client
