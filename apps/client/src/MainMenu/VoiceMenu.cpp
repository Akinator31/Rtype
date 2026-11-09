#include "MainMenu/VoiceMenu.hpp"

#include "Controls/KeyChoice.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/SettingsPage.hpp"
#include "Settings/ControlSettings.hpp"
#include "Settings/NextValue.hpp"
#include "Settings/PlayerSettings.hpp"
#include "Settings/VoiceSettings.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/Talk.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// How much a key or a notch of the wheel changes a volume or the threshold.
constexpr float sliderStep = 0.05f;
/// The longest name of a microphone shown, in characters: the names of the system can be long.
constexpr std::size_t deviceNameLength = 34;
/// The columns of the keys of the voice in the right column, from its left.
constexpr float keysColumn = 400.0f;
constexpr float buttonsColumn = 640.0f;
/// Space between the rows of the keys and the banner of a choice of key under them.
constexpr float bannerGap = 24.0f;

/// The microphone tried while the page is open: its level, on the meter under the threshold.
class MicrophoneTest
{
    voice::Capture &_capture;
    const VoicePreferences &_preferences;
    voice::LevelMeter _meter;
    bool _open = false;

public:
    MicrophoneTest(voice::Capture &capture, const VoicePreferences &preferences)
        : _capture(capture), _preferences(preferences)
    {
    }

    void start()
    {
        _meter.reset();
        _open = _capture.open();
    }

    void stop()
    {
        if (_open) {
            _capture.close();
            _open = false;
        }
    }

    /// Reads what the microphone captured since the last call, with its volume.
    /// @return The level on the scale of the threshold, from 0 to 1.
    float level()
    {
        std::array<std::int16_t, voice::FrameSamples> samples{};
        constexpr float lowest = std::numeric_limits<std::int16_t>::min();
        constexpr float highest = std::numeric_limits<std::int16_t>::max();
        while (const std::size_t count = _open ? _capture.read(samples) : 0) {
            for (std::size_t index = 0; index < count; ++index) {
                const float sample =
                    std::round(static_cast<float>(samples[index]) * _preferences.inputVolume);
                samples[index] = static_cast<std::int16_t>(std::clamp(sample, lowest, highest));
            }
            _meter.push(std::span<const std::int16_t>(samples.data(), count));
        }
        return voice::levelFraction(_meter.level());
    }
};

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

/// A row that goes through values, with its hint.
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

/// A slider of a value from 0 to 1, with its hint.
/// @return The top of the next row.
float addSlider(Page &page, const std::string &captionKey, std::function<float()> get,
                std::function<void(float)> set, const std::string &hintKey, float left, float top)
{
    const ecs::entity_t slider =
        page.interface.slider(page.targets.translations.text(captionKey), std::move(get), std::move(set),
                              sliderStep, {engine::AnchorPoint::TopLeft, left, top});
    page.hints->emplace_back(slider, hintKey);
    return top + page.style.slider.getHeight();
}

/// "Mode: Push-to-talk": push-to-talk, voice activity or off.
float addModeRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    VoicePreferences &preferences = page.targets.voice.preferences;
    const auto value = [&translations, &preferences] {
        return translations.text("voice.mode." + std::string(voice::name(preferences.mode)));
    };
    const auto change = [&preferences](int direction) {
        preferences.mode = stepValue(voice::TalkModes, preferences.mode, direction);
    };
    return addRow(page, "voice.mode", value, change, "voice.hint.mode", left, top);
}

/// The threshold of the voice activity, then the level of the microphone on the same scale under it: the
/// player sees where its voice goes over the threshold.
float addThreshold(Page &page, const std::shared_ptr<MicrophoneTest> &test, float left, float top)
{
    VoicePreferences &preferences = page.targets.voice.preferences;
    top = addSlider(
        page, "voice.threshold", [&preferences] { return voice::levelFraction(preferences.threshold); },
        [&preferences](float value) { preferences.threshold = voice::levelAt(value); },
        "voice.hint.threshold", left, top);
    // Its rail under the rail of the slider
    const engine::SliderStyle &slider = page.style.slider;
    const ecs::entity_t gauge =
        page.interface.gauge(page.targets.translations.text("voice.level"), [test] { return test->level(); },
                             {engine::AnchorPoint::TopLeft, left + slider.inset, top});
    page.ecs.getComponent<engine::Gauge>(gauge).width = slider.getRailWidth();
    page.ecs.getComponent<engine::Extent>(gauge).width = slider.getRailWidth();
    return top + page.style.gauge.getHeight();
}

/// A name of the system cut to deviceNameLength characters, an ellipsis at its end.
std::string shortName(const std::string &name)
{
    if (name.size() <= deviceNameLength) {
        return name;
    }
    std::size_t end = deviceNameLength;
    // Not in the middle of a character of UTF-8
    while (end > 0 && (static_cast<unsigned char>(name[end]) & 0xC0U) == 0x80U) {
        --end;
    }
    return name.substr(0, end) + "…";
}

/// "Microphone: Default": the default of the system, then the microphones it has.
float addDeviceRow(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    VoicePreferences &preferences = page.targets.voice.preferences;
    voice::Microphone &microphone = page.targets.microphone;
    std::vector<std::string> names{""};
    for (std::string &name : microphone.devices()) {
        names.push_back(std::move(name));
    }
    const auto value = [&translations, &preferences] {
        return preferences.inputDevice.empty() ? translations.text("voice.device.default")
                                               : shortName(preferences.inputDevice);
    };
    const auto change = [&preferences, &microphone, names = std::move(names)](int direction) {
        preferences.inputDevice = stepValue(names, preferences.inputDevice, direction);
        microphone.setDevice(preferences.inputDevice);
    };
    return addRow(page, "voice.device", value, change, "voice.hint.device", left, top);
}

/// The volume of the microphone.
float addInputVolume(Page &page, float left, float top)
{
    VoicePreferences &preferences = page.targets.voice.preferences;
    return addSlider(
        page, "voice.input_volume", [&preferences] { return preferences.inputVolume; },
        [&preferences](float volume) { preferences.inputVolume = volume; }, "voice.hint.input_volume", left,
        top);
}

/// "Noise suppression: On" and "Gain control: On": what the microphone goes through before it is sent.
/// @return The top of the next row.
float addProcessingRows(Page &page, float left, float top)
{
    const localization::Localization &translations = page.targets.translations;
    voice::ProcessingSettings &processing = page.targets.voice.preferences.processing;
    const auto onOff = [&translations](const bool &value) {
        return [&translations, &value] { return translations.text(value ? "settings.on" : "settings.off"); };
    };
    top = addRow(
        page, "voice.noise_suppression", onOff(processing.noiseSuppression),
        [&processing](int /*direction*/) { processing.noiseSuppression = !processing.noiseSuppression; },
        "voice.hint.noise_suppression", left, top);
    return addRow(
        page, "voice.gain_control", onOff(processing.gainControl),
        [&processing](int /*direction*/) { processing.gainControl = !processing.gainControl; },
        "voice.hint.gain_control", left, top);
}

/// The volume of the voices of the others.
float addVoiceVolume(Page &page, float left, float top)
{
    VoicePreferences &preferences = page.targets.voice.preferences;
    return addSlider(
        page, "voice.voice_volume", [&preferences] { return preferences.voiceVolume; },
        [&preferences](float volume) { preferences.voiceVolume = volume; }, "voice.hint.voice_volume", left,
        top);
}

/// The rows of the keys of the voice, which open the choice of a key.
/// @return The top of the next row.
float addKeys(Page &page, const SharedKeyChoice &choice, float left, float top)
{
    const KeyTable table{.left = left,
                         .width = page.style.setting.width,
                         .keysColumn = keysColumn,
                         .buttonsColumn = buttonsColumn};
    for (const GameAction &action : VoiceActions) {
        const ecs::entity_t row = addActionRow(page.interface, page.engine, page.targets.translations, action,
                                               choice, table, VoiceKeyCaptureScene, top);
        page.hints->emplace_back(row, "voice.hint." + std::string(action.name));
        top += page.style.keys.rowHeight;
    }
    return top;
}

/// The top of the banner of a choice of key, under the keys of the right column.
float bannerTop(const engine::InterfaceStyle &style)
{
    const float listen = settings_page::labelHeight + style.slider.getHeight() + settings_page::groupGap;
    const float keys =
        settings_page::labelHeight + (static_cast<float>(VoiceActions.size()) * style.keys.rowHeight);
    return settings_page::top + listen + keys + bannerGap;
}

/// On the left, how the player talks and its microphone; on the right, the voices and the keys. The widgets
/// get the focus in the order they are made: down the left column, then down the right one.
void buildVoicePage(engine::Engine &engine, engine::SceneId scene, const engine::InterfaceStyle &style,
                    const SettingsTargets &targets, const SharedKeyChoice &choice,
                    const std::shared_ptr<MicrophoneTest> &test, const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    const localization::Localization &translations = targets.translations;
    engine::InterfaceBuilder interface(ecs, scene, style);
    settings_page::addVeil(interface, ecs);
    interface.boundHeader([&translations] { return translations.text("voice.title"); }, close,
                          {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    Page page{engine, ecs, interface, style, targets, std::make_shared<settings_page::Hints>()};

    const float width = style.setting.width;
    const float left = screenMargin;
    const float right = screenMargin + width + settings_page::columnGap;
    using settings_page::addGroup;
    using settings_page::groupGap;

    targets.microphone.setDevice(targets.voice.preferences.inputDevice);
    test->start();
    float top =
        addGroup(interface, ecs, style, translations, "voice.group.talk", left, settings_page::top, width);
    top = addModeRow(page, left, top);
    top = addThreshold(page, test, left, top) + groupGap;
    top = addGroup(interface, ecs, style, translations, "voice.group.microphone", left, top, width);
    top = addDeviceRow(page, left, top);
    top = addInputVolume(page, left, top);
    addProcessingRows(page, left, top);

    top =
        addGroup(interface, ecs, style, translations, "voice.group.listen", right, settings_page::top, width);
    top = addVoiceVolume(page, right, top) + groupGap;
    top = addGroup(interface, ecs, style, translations, "voice.group.keys", right, top, width);
    top = addKeys(page, choice, right, top);

    // The hint of the focused row, under the banner of a choice of key
    settings_page::addHint(
        interface, ecs, translations, page.hints,
        {engine::AnchorPoint::TopLeft, right + style.setting.inset, top + (3.0f * groupGap)});
    settings_page::addKeys(interface, ecs, translations, "settings.keys");
}

} // namespace

void createVoiceScenes(engine::Engine &engine, const engine::InterfaceStyle &style, SettingsTargets targets)
{
    const auto choice = std::make_shared<KeyChoice>();
    const auto test = std::make_shared<MicrophoneTest>(targets.voice.capture, targets.voice.preferences);
    // Written when the page closes, not at every step of a slider
    const auto close = [&engine, targets, test] {
        test->stop();
        engine::Settings &settings = engine.getSettings();
        keepVoiceSettings(settings, targets.voice.preferences);
        keepControlSettings(settings, engine.getControls());
        writeSettings(settings);
        engine.popScene();
    };
    engine.createScene(VoiceMenuScene, {.overlay = true, .hidesInterfaceBelow = true, .onBack = close},
                       [&style, targets, choice, test, close](engine::Engine &target, engine::SceneId scene) {
                           buildVoicePage(target, scene, style, targets, choice, test, close);
                       });
    createKeyChoiceScene(engine, VoiceKeyCaptureScene, style, targets.translations, choice,
                         screenMargin + style.setting.width + settings_page::columnGap, bannerTop(style));
}

} // namespace rtype::client
