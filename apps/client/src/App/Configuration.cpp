#include "App/Configuration.hpp"

#include "Settings/AuraSettings.hpp"
#include "Settings/ControlSettings.hpp"
#include "Settings/DisplaySettings.hpp"
#include "Settings/PlayerSettings.hpp"
#include "Settings/VoiceSettings.hpp"
#include "rtype/game/Config/GameConfigLoader.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/render/Paths.hpp"

#include <exception>
#include <string>
#include <utility>

namespace rtype::client
{

game::GameConfig readGameConfig(Console &console)
{
    const log::Logger logger("client");
    game::LoadedGameConfig loaded = game::loadGameConfig(render::applicationDirectory() / "config");
    if (loaded.isValid()) {
        return std::move(loaded.config);
    }
    console.print("The game configuration has errors: the game runs without it");
    for (const std::string &error : loaded.errors) {
        console.print(error);
        logger.warning("Configuration: {}", error);
    }
    return {};
}

void readSettings(engine::Settings &settings)
{
    const log::Logger logger("client");
    settings.setPath(render::userDataDirectory() / "settings.txt");
    try {
        settings.readFile();
    } catch (const std::exception &error) {
        logger.warning("Cannot read the settings {}: {}", settings.getPath().string(), error.what());
    }
}

void applySavedSettings(engine::Engine &engine, const SettingsTargets &targets)
{
    const engine::Settings &settings = engine.getSettings();
    applyControlSettings(settings, engine.getControls());
    applyDisplaySettings(settings, targets.window);
    applyPlayerSettings(settings, targets.translations, targets.audio);
    targets.renderer.setColorFilter(readColorFilter(settings));
    targets.auras = readAuraColors(settings);
    targets.voice.preferences = readVoiceSettings(settings);
    targets.microphone.setDevice(targets.voice.preferences.inputDevice);
}

} // namespace rtype::client
