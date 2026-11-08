#include "Settings/VoiceSettings.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

/// The number kept under this key, clamped, or the default if there is none or it is not a number.
float readNumber(const engine::Settings &settings, std::string_view key, float fallback, float low,
                 float high)
{
    const std::optional<float> value = settings.getFloat(key);
    if (!value || !std::isfinite(*value)) {
        return fallback;
    }
    return std::clamp(*value, low, high);
}

} // namespace

VoicePreferences readVoiceSettings(const engine::Settings &settings)
{
    VoicePreferences preferences;
    if (const std::optional<std::string> mode = settings.getString(TalkModeSettingsKey)) {
        if (const std::optional<voice::TalkMode> known = voice::talkModeFromName(*mode)) {
            preferences.mode = *known;
        } else {
            log::Logger("settings")
                .warning("Unknown talk mode \"{}\" in the settings: {} is kept", *mode,
                         voice::name(preferences.mode));
        }
    }
    preferences.threshold = readNumber(settings, ThresholdSettingsKey, preferences.threshold,
                                       voice::MinThreshold, voice::MaxThreshold);
    preferences.inputDevice = settings.getString(InputDeviceSettingsKey).value_or("");
    preferences.inputVolume =
        readNumber(settings, InputVolumeSettingsKey, preferences.inputVolume, 0.0f, voice::DefaultVolume);
    preferences.voiceVolume =
        readNumber(settings, VoiceVolumeSettingsKey, preferences.voiceVolume, 0.0f, voice::DefaultVolume);
    return preferences;
}

void keepVoiceSettings(engine::Settings &settings, const VoicePreferences &preferences)
{
    settings.setString(TalkModeSettingsKey, voice::name(preferences.mode));
    settings.setFloat(ThresholdSettingsKey, preferences.threshold);
    settings.setString(InputDeviceSettingsKey, preferences.inputDevice);
    settings.setFloat(InputVolumeSettingsKey, preferences.inputVolume);
    settings.setFloat(VoiceVolumeSettingsKey, preferences.voiceVolume);
}

} // namespace rtype::client
