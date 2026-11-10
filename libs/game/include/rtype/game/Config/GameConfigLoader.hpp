#pragma once

#include "rtype/game/Config/GameConfig.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace rtype::game
{

/// What loadGameConfig() found.
struct LoadedGameConfig {
    /// Empty if there is any error: a configuration is taken whole or not at all.
    GameConfig config;
    /// One line per problem, naming the file and the field:
    /// "enemies/bydo_scout.lua: health: expected an integer from 1 to 255".
    std::vector<std::string> errors;

    [[nodiscard]] bool isValid() const { return errors.empty(); }
};

/// Reads the configuration of the game from `directory` (ADR 0018,
/// docs/guides/configure-enemies-and-weapons.md): every `.lua` file under its folders projectiles/, weapons/,
/// ships/, enemies/, waves/ and items/, sub-folders included, in alphabetical order, and the file player.lua
/// at its root (required). Each file runs in a sandboxed state of its own and returns the table of one
/// definition. The tables are checked (types, required fields, ranges, unknown fields); once every file is
/// read, the definitions are checked as their registries do, then the ids they name: a weapon its projectile,
/// an enemy its ship and its weapon, a wave its enemies and its items, the player its ship and its weapon.
///
/// The music of a wave that follows it (placement "music") is read from `assets`, at the path `music` of the
/// wave: the loader analyses it to find when the enemies come.
///
/// Every error of a step is reported, not only the first one; the registries and the ids are only checked if
/// every file could be read, so that a broken file does not make those that name it fail too. A missing
/// folder has no definitions; a missing directory, or player.lua, is an error. Nothing is logged: the server
/// and the client say it their own way.
[[nodiscard]] LoadedGameConfig loadGameConfig(const std::filesystem::path &directory,
                                              const std::filesystem::path &assets);

/// The same, with the assets in the folder `assets` next to `directory`: where the client and the server
/// find them beside their executable. Or in `apps/client/assets` next to it, for the configuration of the
/// repository.
[[nodiscard]] LoadedGameConfig loadGameConfig(const std::filesystem::path &directory);

} // namespace rtype::game
