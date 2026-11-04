#pragma once

#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/localization/Localization.hpp"
#include "rtype/render/ColorFilter.hpp"

#include <array>
#include <string_view>

namespace rtype::client
{

/// Where the choices of the Settings page are kept in the settings of the engine.
inline constexpr std::string_view LanguageSettingsKey = "interface.language";
inline constexpr std::string_view MusicVolumeSettingsKey = "audio.music_volume";
inline constexpr std::string_view EffectsVolumeSettingsKey = "audio.effects_volume";
inline constexpr std::string_view ColorFilterSettingsKey = "display.color_filter";

/// The colour filters a player can choose on the Settings page, in the order its button goes through them.
/// The filters that simulate a colour blindness are for the team, not offered there.
inline constexpr std::array<render::ColorFilter, 5> PlayerColorFilters{
    render::ColorFilter::None, render::ColorFilter::Protanopia, render::ColorFilter::Deuteranopia,
    render::ColorFilter::Tritanopia, render::ColorFilter::HighContrast};

/// How a filter of PlayerColorFilters is written in the settings ("deuteranopia"), and the key of its name in
/// the translations ("settings.filter.deuteranopia"). Empty for a filter not offered to the players.
[[nodiscard]] std::string_view colorFilterSettingName(render::ColorFilter filter);
/// The colour filter kept in the settings: ColorFilter::None if there is none, or a value that is not one of
/// PlayerColorFilters.
[[nodiscard]] render::ColorFilter readColorFilter(const engine::Settings &settings);

/// The volumes of a first run, between 0 and 1.
inline constexpr float DefaultMusicVolume = 1.0f;
inline constexpr float DefaultEffectsVolume = 1.0f;

/// Gives the language and the volumes kept in the settings to the translations and the audio. A value that is
/// missing or wrong (a language not loaded, a volume that is not a number) leaves the default: the language
/// the translations started with, DefaultMusicVolume, DefaultEffectsVolume. A volume out of 0..1 is clamped.
void applyPlayerSettings(const engine::Settings &settings, localization::Localization &translations,
                         audio::AudioPlayer &audio);

/// Keeps the current language, volumes and colour filter in the settings, without writing them.
void keepPlayerSettings(engine::Settings &settings, const localization::Localization &translations,
                        const audio::AudioPlayer &audio, render::ColorFilter filter);

/// Writes the settings to their file. A file that cannot be written is logged: the choices then only last
/// until the client closes.
void writeSettings(engine::Settings &settings);

} // namespace rtype::client
