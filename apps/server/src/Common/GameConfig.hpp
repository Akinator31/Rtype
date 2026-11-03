#pragma once

#include "rtype/game/Config/GameConfig.hpp"
#include "rtype/log/Logger.hpp"

#include <filesystem>
#include <optional>

namespace rtype::server
{

/// The directory of the configuration of the game: `option` (--config) if given, else config/ next to the
/// executable, where the build and the packages put it.
[[nodiscard]] std::filesystem::path getConfigDirectory(const std::filesystem::path &option);

/// Loads the configuration of the game (ADR 0018), logging each error with its file and field. The server
/// does not start with a configuration that has errors: nothing is returned then. The waves of the final boss
/// (MaxWaveDifficulty) are left out: they are played offline only, in the story mode of the client.
[[nodiscard]] std::optional<rtype::game::GameConfig> readGameConfig(const std::filesystem::path &directory,
                                                                    const rtype::log::Logger &logger);

} // namespace rtype::server
