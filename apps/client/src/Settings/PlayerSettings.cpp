#include "Settings/PlayerSettings.hpp"

#include "rtype/log/Logger.hpp"

#include <cmath>
#include <exception>
#include <optional>
#include <string>

namespace rtype::client
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("settings");
    return instance;
}

/// The volume kept under this key, or the default if there is none or it is not a number.
float readVolume(const engine::Settings &settings, std::string_view key, float fallback)
{
    const std::optional<float> volume = settings.getFloat(key);
    if (!volume || !std::isfinite(*volume)) {
        return fallback;
    }
    return *volume;
}

} // namespace

std::string_view colorFilterSettingName(render::ColorFilter filter)
{
    switch (filter) {
    case render::ColorFilter::None:
        return "none";
    case render::ColorFilter::Protanopia:
        return "protanopia";
    case render::ColorFilter::Deuteranopia:
        return "deuteranopia";
    case render::ColorFilter::Tritanopia:
        return "tritanopia";
    case render::ColorFilter::HighContrast:
        return "high_contrast";
    default:
        return {};
    }
}

render::ColorFilter readColorFilter(const engine::Settings &settings)
{
    const std::optional<std::string> name = settings.getString(ColorFilterSettingsKey);
    if (!name) {
        return render::ColorFilter::None;
    }
    for (const render::ColorFilter filter : PlayerColorFilters) {
        if (colorFilterSettingName(filter) == *name) {
            return filter;
        }
    }
    logger().warning("Unknown colour filter \"{}\" in the settings: none is applied", *name);
    return render::ColorFilter::None;
}

void applyPlayerSettings(const engine::Settings &settings, localization::Localization &translations,
                         audio::AudioPlayer &audio)
{
    if (const std::optional<std::string> language = settings.getString(LanguageSettingsKey)) {
        if (!translations.setLanguage(*language)) {
            logger().warning("Unknown language \"{}\" in the settings: {} is kept", *language,
                             translations.language());
        }
    }
    audio.setMusicVolume(readVolume(settings, MusicVolumeSettingsKey, DefaultMusicVolume));
    audio.setEffectsVolume(readVolume(settings, EffectsVolumeSettingsKey, DefaultEffectsVolume));
}

void keepPlayerSettings(engine::Settings &settings, const localization::Localization &translations,
                        const audio::AudioPlayer &audio, render::ColorFilter filter)
{
    settings.setString(LanguageSettingsKey, translations.language());
    settings.setFloat(MusicVolumeSettingsKey, audio.musicVolume());
    settings.setFloat(EffectsVolumeSettingsKey, audio.effectsVolume());
    if (const std::string_view name = colorFilterSettingName(filter); !name.empty()) {
        settings.setString(ColorFilterSettingsKey, name);
    }
}

void writeSettings(engine::Settings &settings)
{
    try {
        settings.writeFile();
    } catch (const std::exception &error) {
        logger().error("Cannot write the settings {}: {}", settings.getPath().string(), error.what());
    }
}

} // namespace rtype::client
