#pragma once

#include "Voice/VoiceDevices.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <string_view>

namespace rtype::client
{

/// Where the choices of the Voice page are kept in the settings of the engine.
inline constexpr std::string_view TalkModeSettingsKey = "voice.mode";
inline constexpr std::string_view ThresholdSettingsKey = "voice.threshold";
inline constexpr std::string_view InputDeviceSettingsKey = "voice.input_device";
inline constexpr std::string_view InputVolumeSettingsKey = "voice.input_volume";
inline constexpr std::string_view VoiceVolumeSettingsKey = "voice.volume";
/// The choices for each other player: `voice.player.<key>.volume` and `voice.player.<key>.muted`, the key
/// being playerVoiceKey().
inline constexpr std::string_view PlayerVoiceSettingsPrefix = "voice.player.";

/// The preferences of the voice kept in the settings. A value that is missing or wrong (a mode that is not
/// one, a number that is not one) leaves its default of VoicePreferences; a threshold out of
/// [voice::MinThreshold, voice::MaxThreshold] and a volume out of [0, 1] are clamped. The choices for the
/// other players are read too.
[[nodiscard]] VoicePreferences readVoiceSettings(const engine::Settings &settings);
/// Keeps the preferences of the voice in the settings, without writing them. A player heard as by default is
/// not kept.
void keepVoiceSettings(engine::Settings &settings, const VoicePreferences &preferences);

} // namespace rtype::client
