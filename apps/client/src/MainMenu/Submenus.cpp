#include "MainMenu/Submenus.hpp"

#include "MainMenu/Layout.hpp"
#include "MainMenu/UiShowcase.hpp"
#include "Settings/PlayerSettings.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include <memory>
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

/// The rows of the Settings page, under the header: the language, the two volumes, the colour filter, then
/// the showcase.
constexpr float volumesTop = buttonsTop + buttonsSpacing + 20.0f;
constexpr float volumesSpacing = 96.0f;
constexpr float filterTop = volumesTop + (2.0f * volumesSpacing) + 20.0f;
constexpr float showcaseTop = filterTop + buttonsSpacing;
/// How much a key or a notch of the wheel changes a volume.
constexpr float volumeStep = 0.05f;

void createSettingsScene(engine::Engine &engine, const engine::InterfaceStyle &style,
                         localization::Localization &translations, audio::AudioPlayer &audio,
                         render::Renderer &renderer)
{
    // Each choice applies at once; the settings are written when the page closes, not at every step of a slider
    const auto close = [&engine, &translations, &audio, &renderer] {
        writePlayerSettings(engine.getSettings(), translations, audio, renderer.colorFilter());
        engine.popScene();
    };
    engine.createScene(
        SettingsMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
        [&style, &translations, &audio, &renderer, close](engine::Engine &target, engine::SceneId scene) {
            ecs::Ecs &ecs = target.getEcs();
            engine::InterfaceBuilder interface(ecs, scene, style);
            interface.boundHeader([&translations] { return translations.text("menu.settings"); }, close,
                                  {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});

            // The captions of the sliders are plain text: they are written again when the language changes
            const auto volumes = std::make_shared<std::vector<std::pair<ecs::entity_t, std::string>>>();
            const auto label = [&translations] {
                const std::string key =
                    translations.language() == "fr" ? "language.french" : "language.english";
                return translations.text("settings.language") + ": " + translations.text(key);
            };
            const ecs::entity_t choice = interface.button(
                label(),
                [&ecs, &translations, volumes] {
                    translations.setLanguage(translations.language() == "fr" ? "en" : "fr");
                    for (const auto &[slider, key] : *volumes) {
                        ecs.getComponent<engine::Slider>(slider).caption = translations.text(key);
                    }
                },
                {engine::AnchorPoint::TopLeft, screenMargin, buttonsTop});
            ecs.addComponent<engine::BoundText>(choice, label);

            // The area of a slider goes beyond its rail by the handle: the rail starts at the margin, like the
            // buttons
            const float sliderLeft = screenMargin - style.slider.handleDragRadius;
            volumes->emplace_back(
                interface.slider(
                    translations.text("settings.music_volume"), [&audio] { return audio.musicVolume(); },
                    [&audio](float volume) { audio.setMusicVolume(volume); }, volumeStep,
                    {engine::AnchorPoint::TopLeft, sliderLeft, volumesTop}),
                "settings.music_volume");
            volumes->emplace_back(
                interface.slider(
                    translations.text("settings.effects_volume"), [&audio] { return audio.effectsVolume(); },
                    [&audio](float volume) { audio.setEffectsVolume(volume); }, volumeStep,
                    {engine::AnchorPoint::TopLeft, sliderLeft, volumesTop + volumesSpacing}),
                "settings.effects_volume");

            const auto filterLabel = [&translations, &renderer] {
                const std::string name(colorFilterSettingName(renderer.colorFilter()));
                return translations.text("settings.color_filter") + ": " +
                       translations.text("settings.filter." + (name.empty() ? std::string("none") : name));
            };
            const ecs::entity_t filter = interface.button(
                filterLabel(),
                [&renderer] { renderer.setColorFilter(nextColorFilter(renderer.colorFilter())); },
                {engine::AnchorPoint::TopLeft, screenMargin, filterTop});
            ecs.addComponent<engine::BoundText>(filter, filterLabel);

            const ecs::entity_t showcase = interface.button(
                translations.text("showcase.open"), [&target] { target.pushScene(UiShowcaseScene); },
                {engine::AnchorPoint::TopLeft, screenMargin, showcaseTop});
            ecs.addComponent<engine::BoundText>(
                showcase, [&translations] { return translations.text("showcase.open"); });
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
    createSettingsScene(engine, style, translations, audio, renderer);
    createUiShowcaseScene(engine, style, translations);
}

} // namespace rtype::client
