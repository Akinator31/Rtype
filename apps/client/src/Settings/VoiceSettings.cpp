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

constexpr std::string_view VolumeSuffix = ".volume";
constexpr std::string_view MutedSuffix = ".muted";

/// The choices for the other players, from their keys.
void readPlayerVoices(const engine::Settings &settings, VoicePreferences &preferences)
{
    for (const std::string &key : settings.keys(PlayerVoiceSettingsPrefix)) {
        const std::string_view rest = std::string_view(key).substr(PlayerVoiceSettingsPrefix.size());
        if (rest.ends_with(VolumeSuffix)) {
            const std::string player(rest.substr(0, rest.size() - VolumeSuffix.size()));
            preferences.players[player].volume = readNumber(settings, key, voice::DefaultVolume, 0.0f, 1.0f);
        } else if (rest.ends_with(MutedSuffix)) {
            const std::string player(rest.substr(0, rest.size() - MutedSuffix.size()));
            preferences.players[player].muted = settings.getBool(key).value_or(false);
        }
    }
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
    readPlayerVoices(settings, preferences);
    preferences.processing.noiseSuppression =
        settings.getBool(NoiseSuppressionSettingsKey).value_or(preferences.processing.noiseSuppression);
    preferences.processing.gainControl =
        settings.getBool(GainControlSettingsKey).value_or(preferences.processing.gainControl);
    return preferences;
}

void keepVoiceSettings(engine::Settings &settings, const VoicePreferences &preferences)
{
    settings.setString(TalkModeSettingsKey, voice::name(preferences.mode));
    settings.setFloat(ThresholdSettingsKey, preferences.threshold);
    settings.setString(InputDeviceSettingsKey, preferences.inputDevice);
    settings.setFloat(InputVolumeSettingsKey, preferences.inputVolume);
    settings.setFloat(VoiceVolumeSettingsKey, preferences.voiceVolume);
    for (const std::string &key : settings.keys(PlayerVoiceSettingsPrefix)) {
        settings.removeKey(key);
    }
    for (const auto &[player, choice] : preferences.players) {
        if (choice == PlayerVoice{}) {
            continue;
        }
        const std::string prefix = std::string(PlayerVoiceSettingsPrefix) + player;
        settings.setFloat(prefix + std::string(VolumeSuffix), choice.volume);
        settings.setBool(prefix + std::string(MutedSuffix), choice.muted);
    }
    settings.setBool(NoiseSuppressionSettingsKey, preferences.processing.noiseSuppression);
    settings.setBool(GainControlSettingsKey, preferences.processing.gainControl);
}

} // namespace rtype::client
