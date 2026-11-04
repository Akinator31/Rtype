#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "MainMenu/UiShowcase.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <memory>
#include <string>
#include <string_view>
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

/// A button in the column on the left whose text is written from a function every frame, so that it follows
/// the language and the value it shows.
ecs::entity_t boundButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                          std::function<std::string()> label, std::function<void()> onPress, float top)
{
    const ecs::entity_t button =
        interface.button(label(), std::move(onPress), {engine::AnchorPoint::TopLeft, screenMargin, top});
    ecs.addComponent<engine::BoundText>(button, std::move(label));
    return button;
}

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
                           float top = buttonsTop;
                           for (const SubmenuButton &button : buttons) {
                               boundButton(
                                   interface, target.getEcs(),
                                   [&translations, key = button.key] { return translations.text(key); },
                                   button.onPress, top);
                               top += buttonsSpacing;
                           }
                       });
}

/// The rows of the Settings page, under the header: the language, the two volumes, the colour filter, then
/// the showcase.
constexpr float volumesTop = buttonsTop + buttonsSpacing + 20.0f;
constexpr float volumesSpacing = 96.0f;
constexpr float filterTop = volumesTop + (2.0f * volumesSpacing) + 20.0f;
constexpr float showcaseTop = filterTop + buttonsSpacing;
/// How much a key or a notch of the wheel changes a volume.
constexpr float volumeStep = 0.05f;

/// What the Settings page changes.
struct SettingsTargets {
    localization::Localization &translations;
    audio::AudioPlayer &audio;
    render::Renderer &renderer;
};

/// The sliders of a page, with the key of their caption: their caption is plain text, written again when the
/// language changes.
using SliderCaptions = std::vector<std::pair<ecs::entity_t, std::string>>;

/// "Language: French", a button that switches between English and French, and writes the captions of the
/// sliders again in the new language.
void addLanguageButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs,
                       localization::Localization &translations,
                       std::shared_ptr<const SliderCaptions> sliders)
{
    const auto label = [&translations] {
        const char *language = translations.language() == "fr" ? "language.french" : "language.english";
        return translations.text("settings.language") + ": " + translations.text(language);
    };
    const auto next = [&ecs, &translations, sliders = std::move(sliders)] {
        translations.setLanguage(translations.language() == "fr" ? "en" : "fr");
        for (const auto &[slider, key] : *sliders) {
            ecs.getComponent<engine::Slider>(slider).caption = translations.text(key);
        }
    };
    boundButton(interface, ecs, label, next, buttonsTop);
}

/// A slider of a volume, in the column on the left, with the key of its caption.
std::pair<ecs::entity_t, std::string> volumeSlider(engine::InterfaceBuilder &interface,
                                                   const engine::InterfaceStyle &style,
                                                   const localization::Localization &translations,
                                                   const std::string &key, std::function<float()> get,
                                                   std::function<void(float)> set, float top)
{
    // The area of a slider goes beyond its rail by the handle: the rail starts at the margin, like the
    // buttons
    const float left = screenMargin - style.slider.handleDragRadius;
    return {interface.slider(translations.text(key), std::move(get), std::move(set), volumeStep,
                             {engine::AnchorPoint::TopLeft, left, top}),
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
                [&audio](float volume) { audio.setMusicVolume(volume); }, volumesTop),
            volumeSlider(
                interface, style, targets.translations, "settings.effects_volume",
                [&audio] { return audio.effectsVolume(); },
                [&audio](float volume) { audio.setEffectsVolume(volume); }, volumesTop + volumesSpacing)};
}

/// "Colour filter: Deuteranopia", a button that goes through the filters of PlayerColorFilters.
void addColorFilterButton(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const SettingsTargets &targets)
{
    localization::Localization &translations = targets.translations;
    render::Renderer &renderer = targets.renderer;
    const auto label = [&translations, &renderer] {
        const std::string_view name = colorFilterSettingName(renderer.colorFilter());
        return translations.text("settings.color_filter") + ": " +
               translations.text("settings.filter." + std::string(name.empty() ? "none" : name));
    };
    boundButton(
        interface, ecs, label,
        [&renderer] { renderer.setColorFilter(nextColorFilter(renderer.colorFilter())); }, filterTop);
}

void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style, SettingsTargets targets)
{
    // Each choice applies at once; the settings are written when the page closes, not at every step of a
    // slider
    const auto close = [&engine, targets] {
        writePlayerSettings(engine.getSettings(), targets.translations, targets.audio,
                            targets.renderer.colorFilter());
        engine.popScene();
    };
    engine.createScene(SettingsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, targets, close](engine::Engine &target, engine::SceneId scene) {
                           ecs::Ecs &ecs = target.getEcs();
                           localization::Localization &translations = targets.translations;
                           engine::InterfaceBuilder interface(ecs, scene, style);
                           interface.boundHeader(
                               [&translations] { return translations.text("menu.settings"); }, close,
                               {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
                           // The buttons get the focus in the order they are made: the sliders are known
                           // after the language
                           const auto sliders = std::make_shared<SliderCaptions>();
                           addLanguageButton(interface, ecs, translations, sliders);
                           *sliders = addVolumeSliders(interface, style, targets);
                           addColorFilterButton(interface, ecs, targets);
                           boundButton(
                               interface, ecs, [&translations] { return translations.text("showcase.open"); },
                               [&target] { target.pushScene(UiShowcaseScene); }, showcaseTop);
                       });
}

} // namespace

void createSubmenuScenes(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations, audio::AudioPlayer &audio,
                         render::Renderer &renderer, std::function<void()> playSolo,
                         std::function<void()> openHangar)
{
    createSubmenuScene(engine, style, PlayMenuScene, translations, "menu.play",
                       {{"menu.solo", std::move(playSolo)}, {"menu.ships", std::move(openHangar)}});
    createSettingsScene(engine, style, {translations, audio, renderer});
    createUiShowcaseScene(engine, style, translations);
}

} // namespace rtype::client
