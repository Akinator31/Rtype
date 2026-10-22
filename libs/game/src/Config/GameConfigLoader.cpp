#include "rtype/game/Config/GameConfigLoader.hpp"

#include "Config/DefinitionReaders.hpp"
#include "Config/TableReader.hpp"
#include "rtype/game/Enemy/EnemyRegistry.hpp"
#include "rtype/game/Projectile/ProjectileRegistry.hpp"
#include "rtype/game/Ship/ShipRegistry.hpp"
#include "rtype/game/Wave/WaveRegistry.hpp"
#include "rtype/game/Weapon/WeaponRegistry.hpp"
#include "rtype/scripting/State/ScriptError.hpp"
#include "rtype/scripting/State/State.hpp"

#include <algorithm>
#include <format>
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

/// Every definition of the files, with the file of each one.
struct ReadFiles {
    std::vector<FromFile<ProjectileDefinition>> projectiles;
    std::vector<FromFile<WeaponDefinition>> weapons;
    std::vector<FromFile<ShipDefinition>> ships;
    std::vector<FromFile<EnemyDefinition>> enemies;
    std::vector<FromFile<WaveDefinition>> waves;
};

ReadFiles readFiles(const std::filesystem::path &directory, std::vector<std::string> &errors)
{
    ReadFiles files;
    files.projectiles =
        readFolder<ProjectileDefinition>(directory, "projectiles", readProjectileDefinition, errors);
    files.weapons = readFolder<WeaponDefinition>(directory, "weapons", readWeaponDefinition, errors);
    files.ships = readFolder<ShipDefinition>(directory, "ships", readShipDefinition, errors);
    files.enemies = readFolder<EnemyDefinition>(directory, "enemies", readEnemyDefinition, errors);
    files.waves = readFolder<WaveDefinition>(directory, "waves", readWaveDefinition, errors);
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
}

GameConfig takeConfig(ReadFiles &files)
{
    return {takeDefinitions(files.projectiles), takeDefinitions(files.weapons), takeDefinitions(files.ships),
            takeDefinitions(files.enemies), takeDefinitions(files.waves)};
}

} // namespace

LoadedGameConfig loadGameConfig(const std::filesystem::path &directory)
{
    LoadedGameConfig loaded;
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        loaded.errors.push_back(std::format("{}: no configuration directory", directory.generic_string()));
        return loaded;
    }
    ReadFiles files = readFiles(directory, loaded.errors);
    if (loaded.errors.empty()) {
        checkDefinitions(files, loaded.errors);
    }
    if (loaded.errors.empty()) {
        loaded.config = takeConfig(files);
    }
    return loaded;
}

} // namespace rtype::game
