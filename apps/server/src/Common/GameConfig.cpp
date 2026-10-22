#include "Common/GameConfig.hpp"

#include "Master/Process.hpp"
#include "rtype/game/Config/GameConfigLoader.hpp"

#include <string>
#include <utility>

namespace rtype::server
{

std::filesystem::path getConfigDirectory(const std::filesystem::path &option)
{
    if (!option.empty()) {
        return option;
    }
    return currentExecutable().parent_path() / "config";
}

std::optional<rtype::game::GameConfig> readGameConfig(const std::filesystem::path &directory,
                                                      const rtype::log::Logger &logger)
{
    rtype::game::LoadedGameConfig loaded = rtype::game::loadGameConfig(directory);
    if (!loaded.isValid()) {
        for (const std::string &error : loaded.errors) {
            logger.error("Configuration: {}", error);
        }
        logger.error("The game configuration in {} is not valid: fix it, or give another one with --config",
                     directory.string());
        return std::nullopt;
    }
    const rtype::game::GameConfig &config = loaded.config;
    logger.info("Game configuration {}: {} projectiles, {} weapons, {} ships, {} enemies, {} waves",
                directory.string(), config.projectiles.size(), config.weapons.size(), config.ships.size(),
                config.enemies.size(), config.waves.size());
    return std::move(loaded.config);
}

} // namespace rtype::server
