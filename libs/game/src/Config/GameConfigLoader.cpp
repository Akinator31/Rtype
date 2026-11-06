#include "rtype/game/Config/GameConfigLoader.hpp"

#include "Config/DefinitionReaders.hpp"
#include "Config/TableReader.hpp"
#include "Wave/MusicTimeline.hpp"
#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Projectile/ProjectileRegistry.hpp"
#include "rtype/game/Ship/ShipRegistry.hpp"
#include "rtype/game/Wave/WaveRegistry.hpp"
#include "rtype/game/Weapon/WeaponRegistry.hpp"
#include "rtype/scripting/State/ScriptError.hpp"
#include "rtype/scripting/State/State.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <future>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rtype::game
{

namespace
{

/// A definition and the file it comes from, for the errors found after it was read.
template <typename Definition>
struct FromFile {
    std::string file;
    Definition definition;
};

template <typename Definition>
using DefinitionReader = Definition (*)(TableReader &);

/// The file of the player, at the root of the directory.
constexpr std::string_view PlayerFile = "player.lua";

/// The `.lua` files under `folder`, by path, so that every machine reads them in the same order.
std::vector<std::filesystem::path> listScripts(const std::filesystem::path &folder,
                                               std::vector<std::string> &errors)
{
    std::vector<std::filesystem::path> scripts;
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) {
        return scripts;
    }
    for (std::filesystem::recursive_directory_iterator it(folder, error), end; !error && it != end;
         it.increment(error)) {
        if (it->is_regular_file(error) && it->path().extension() == ".lua") {
            scripts.push_back(it->path());
        }
    }
    if (error) {
        errors.push_back(std::format("{}: cannot be listed: {}", folder.generic_string(), error.message()));
    }
    std::ranges::sort(scripts);
    return scripts;
}

/// Runs one file in a state of its own and reads the table it returns.
template <typename Definition>
std::optional<Definition> readScript(const std::filesystem::path &path, const std::string &name,
                                     DefinitionReader<Definition> read, std::vector<std::string> &errors)
{
    scripting::State state;
    try {
        const luabridge::LuaRef result = state.runFile(path, name);
        if (!result.isTable()) {
            errors.push_back(
                std::format("{}: expected the file to return a table, `return {{ ... }}`", name));
            return std::nullopt;
        }
        TableReader table(result, "");
        return read(table);
    } catch (const scripting::ScriptError &error) {
        errors.emplace_back(error.what());
    } catch (const ConfigError &error) {
        errors.push_back(std::format("{}: {}", name, error.what()));
    }
    return std::nullopt;
}

template <typename Definition>
std::vector<FromFile<Definition>> readFolder(const std::filesystem::path &directory,
                                             const std::string &folder, DefinitionReader<Definition> read,
                                             std::vector<std::string> &errors)
{
    std::vector<FromFile<Definition>> definitions;
    for (const std::filesystem::path &path : listScripts(directory / folder, errors)) {
        const std::string name = path.lexically_relative(directory).generic_string();
        if (std::optional<Definition> definition = readScript(path, name, read, errors)) {
            definitions.push_back({name, std::move(*definition)});
        }
    }
    return definitions;
}

/// Adds each definition to the registry, which checks it; a refused one is an error of its file.
template <typename Definition, typename Add>
void registerAll(const std::vector<FromFile<Definition>> &definitions, Add add,
                 std::vector<std::string> &errors)
{
    for (const FromFile<Definition> &read : definitions) {
        try {
            add(read.definition);
        } catch (const std::invalid_argument &error) {
            errors.push_back(std::format("{}: {}", read.file, error.what()));
        }
    }
}

void checkReference(bool known, const std::string &file, std::string_view field, std::string_view kind,
                    const std::string &id, std::vector<std::string> &errors)
{
    if (!known) {
        errors.push_back(std::format("{}: {}: unknown {} '{}'", file, field, kind, id));
    }
}

template <typename Definition>
std::vector<Definition> takeDefinitions(std::vector<FromFile<Definition>> &read)
{
    std::vector<Definition> definitions;
    definitions.reserve(read.size());
    for (FromFile<Definition> &item : read) {
        definitions.push_back(std::move(item.definition));
    }
    return definitions;
}

/// Gives each enemy whose file has an `update` function the code of its file: its behavior is run again by
/// each Simulation, in a state of its own.
void readBehaviors(const std::filesystem::path &directory, std::vector<FromFile<EnemyDefinition>> &enemies,
                   std::vector<std::string> &errors)
{
    for (auto &[file, enemy] : enemies) {
        if (!enemy.behavior) {
            continue;
        }
        std::ifstream stream(directory / file, std::ios::binary);
        if (!stream) {
            errors.push_back(std::format("{}: cannot be read", file));
            continue;
        }
        enemy.behavior = EnemyBehavior{
            file, std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>())};
    }
}

/// Every definition of the files, with the file of each one.
struct ReadFiles {
    std::vector<FromFile<ProjectileDefinition>> projectiles;
    std::vector<FromFile<WeaponDefinition>> weapons;
    std::vector<FromFile<ShipDefinition>> ships;
    std::vector<FromFile<EnemyDefinition>> enemies;
    std::vector<FromFile<WaveDefinition>> waves;
    std::optional<FromFile<PlayerDefinition>> player;
};

/// The assets of the programs, next to the configuration: `assets/` beside `config/` once built or installed,
/// or `apps/client/assets/` when the configuration is the one of the repository.
std::filesystem::path findAssetsBeside(const std::filesystem::path &directory)
{
    const std::filesystem::path normal = directory.lexically_normal();
    const std::filesystem::path root =
        normal.has_filename() ? normal.parent_path() : normal.parent_path().parent_path();
    std::error_code error;
    if (!std::filesystem::is_directory(root / "assets", error) &&
        std::filesystem::is_directory(root / "apps" / "client" / "assets", error)) {
        return root / "apps" / "client" / "assets";
    }
    return root / "assets";
}

/// Finds the enemies of the waves that follow a music: the analysis of the music needs the assets. Each music
/// is analysed on a thread of its own, they are long and independent.
void analyseMusics(const std::filesystem::path &assets, std::vector<FromFile<WaveDefinition>> &waves,
                   std::vector<std::string> &errors)
{
    struct Analysis {
        const std::string *file;
        std::future<void> done;
    };
    std::vector<Analysis> analyses;
    for (auto &wave : waves) {
        if (wave.definition.placement == WavePlacement::Music) {
            analyses.push_back({&wave.file, std::async(std::launch::async, [&wave, &assets] {
                                    fillMusicTimeline(wave.definition, assets);
                                })});
        }
    }
    for (auto &[file, done] : analyses) {
        try {
            done.get();
        } catch (const std::runtime_error &error) {
            errors.push_back(std::format("{}: music: {}", *file, error.what()));
        }
    }
}

/// The file of the player, at the root of the directory: required, a game cannot be played without it.
std::optional<FromFile<PlayerDefinition>> readPlayerFile(const std::filesystem::path &directory,
                                                         std::vector<std::string> &errors)
{
    const std::string name(PlayerFile);
    std::error_code error;
    if (!std::filesystem::is_regular_file(directory / name, error)) {
        errors.push_back(std::format("{}: missing: the players need a ship and a weapon", name));
        return std::nullopt;
    }
    std::optional<PlayerDefinition> player = readScript(directory / name, name, readPlayerDefinition, errors);
    if (player) {
        return FromFile<PlayerDefinition>{name, std::move(*player)};
    }
    return std::nullopt;
}

ReadFiles readFiles(const std::filesystem::path &directory, const std::filesystem::path &assets,
                    std::vector<std::string> &errors)
{
    ReadFiles files;
    files.projectiles =
        readFolder<ProjectileDefinition>(directory, "projectiles", readProjectileDefinition, errors);
    files.weapons = readFolder<WeaponDefinition>(directory, "weapons", readWeaponDefinition, errors);
    files.ships = readFolder<ShipDefinition>(directory, "ships", readShipDefinition, errors);
    files.enemies = readFolder<EnemyDefinition>(directory, "enemies", readEnemyDefinition, errors);
    readBehaviors(directory, files.enemies, errors);
    files.waves = readFolder<WaveDefinition>(directory, "waves", readWaveDefinition, errors);
    if (errors.empty()) {
        analyseMusics(assets, files.waves, errors);
    }
    files.player = readPlayerFile(directory, errors);
    return files;
}

/// Checks the definitions as their registries do, then the ids they name. Only called once every file is read
/// without error: a file that failed would make those that name it fail too, with misleading errors.
void checkDefinitions(const ReadFiles &files, std::vector<std::string> &errors)
{
    ProjectileRegistry projectiles;
    WeaponRegistry weapons;
    ShipRegistry ships;
    EnemyRegistry enemies;
    WaveRegistry waves;
    registerAll(
        files.projectiles, [&](const auto &definition) { projectiles.addDefinition(definition); }, errors);
    registerAll(files.weapons, [&](const auto &definition) { weapons.addDefinition(definition); }, errors);
    registerAll(files.ships, [&](const auto &definition) { ships.addDefinition(definition); }, errors);
    registerAll(files.enemies, [&](const auto &definition) { enemies.addDefinition(definition); }, errors);
    registerAll(
        files.waves, [&](const auto &definition) { waves.addDefinition(definition, enemies); }, errors);

    for (const auto &[file, weapon] : files.weapons) {
        checkReference(projectiles.hasDefinition(weapon.projectile), file, "projectile", "projectile",
                       weapon.projectile, errors);
    }
    for (const auto &[file, enemy] : files.enemies) {
        checkReference(ships.hasDefinition(enemy.ship), file, "ship", "ship", enemy.ship, errors);
        checkReference(weapons.hasDefinition(enemy.weapon), file, "weapon", "weapon", enemy.weapon, errors);
    }
    const auto &[file, player] = *files.player;
    checkReference(ships.hasDefinition(player.ship), file, "ship", "ship", player.ship, errors);
    checkReference(weapons.hasDefinition(player.weapon), file, "weapon", "weapon", player.weapon, errors);
}

GameConfig takeConfig(ReadFiles &files)
{
    return {takeDefinitions(files.projectiles), takeDefinitions(files.weapons),
            takeDefinitions(files.ships),       takeDefinitions(files.enemies),
            takeDefinitions(files.waves),       std::move(files.player->definition)};
}

} // namespace

LoadedGameConfig loadGameConfig(const std::filesystem::path &directory)
{
    return loadGameConfig(directory, findAssetsBeside(directory));
}

LoadedGameConfig loadGameConfig(const std::filesystem::path &directory, const std::filesystem::path &assets)
{
    LoadedGameConfig loaded;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        loaded.errors.push_back(std::format("{}: no configuration directory", directory.generic_string()));
        return loaded;
    }
    ReadFiles files = readFiles(directory, assets, loaded.errors);
    if (loaded.errors.empty()) {
        checkDefinitions(files, loaded.errors);
    }
    if (loaded.errors.empty()) {
        loaded.config = takeConfig(files);
    }
    return loaded;
}

} // namespace rtype::game
