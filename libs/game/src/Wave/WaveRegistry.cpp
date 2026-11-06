#include "rtype/game/Wave/WaveRegistry.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

namespace rtype::game
{

namespace
{

void checkEnemy(const WaveDefinition &definition, const EnemyRegistry &enemies, const std::string &enemy)
{
    if (!enemies.hasDefinition(enemy)) {
        throw std::invalid_argument(
            std::format("the wave '{}' uses the unknown enemy '{}'", definition.name, enemy));
    }
}

void checkManual(const WaveDefinition &definition, const EnemyRegistry &enemies)
{
    if (!definition.enemy.empty()) {
        throw std::invalid_argument(
            std::format("the manual wave '{}' lists its enemies in spawns, not in enemy", definition.name));
    }
    if (definition.spawns.size() != definition.quantity) {
        throw std::invalid_argument(
            std::format("the manual wave '{}' needs as many spawns as its quantity", definition.name));
    }
    for (const WaveSpawn &spawn : definition.spawns) {
        checkEnemy(definition, enemies, spawn.enemy);
    }
}

void checkRandom(const WaveDefinition &definition, const EnemyRegistry &enemies)
{
    if (!definition.spawns.empty()) {
        throw std::invalid_argument(std::format(
            "the random wave '{}' gets its positions from its area, not from spawns", definition.name));
    }
    if (definition.enemy.empty()) {
        throw std::invalid_argument(std::format("the random wave '{}' needs an enemy", definition.name));
    }
    checkEnemy(definition, enemies, definition.enemy);
    const WaveArea &area = definition.area;
    if (!(area.minX <= area.maxX) || !(area.minY <= area.maxY)) {
        throw std::invalid_argument(std::format("the wave '{}' has an empty area", definition.name));
    }
}

void checkMusic(const WaveDefinition &definition, const EnemyRegistry &enemies)
{
    if (definition.music.empty()) {
        throw std::invalid_argument(std::format("the music wave '{}' needs a music", definition.name));
    }
    if (!definition.spawns.empty() || !definition.enemy.empty()) {
        throw std::invalid_argument(
            std::format("the music wave '{}' gets its enemies from its beats, not from spawns nor enemy",
                        definition.name));
    }
    if (definition.timeline.size() != definition.quantity) {
        throw std::invalid_argument(
            std::format("the music wave '{}' needs as many enemies as its timeline", definition.name));
    }
    const bool hasEnemies =
        std::ranges::any_of(definition.beats, [](const WaveBeatRule &rule) { return !rule.enemies.empty(); });
    if (!hasEnemies) {
        throw std::invalid_argument(
            std::format("the music wave '{}' needs enemies on its beats", definition.name));
    }
    for (const WaveBeatRule &rule : definition.beats) {
        for (const std::string &enemy : rule.enemies) {
            checkEnemy(definition, enemies, enemy);
        }
    }
    if (!(definition.lead >= 0.0f)) {
        throw std::invalid_argument(std::format("the wave '{}' needs a positive lead", definition.name));
    }
    const WaveArea &area = definition.area;
    if (!(area.minX <= area.maxX) || !(area.minY <= area.maxY)) {
        throw std::invalid_argument(std::format("the wave '{}' has an empty area", definition.name));
    }
}

void checkDefinition(const WaveDefinition &definition, const EnemyRegistry &enemies)
{
    if (definition.name.empty()) {
        throw std::invalid_argument("a wave needs a name");
    }
    if (definition.quantity == 0) {
        throw std::invalid_argument(std::format("the wave '{}' needs at least one enemy", definition.name));
    }
    if (definition.difficulty < MinWaveDifficulty || definition.difficulty > MaxWaveDifficulty) {
        throw std::invalid_argument(std::format("the wave '{}' needs a difficulty from {} to {}",
                                                definition.name, MinWaveDifficulty, MaxWaveDifficulty));
    }
    if (!(definition.spawnInterval >= 0.0f)) {
        throw std::invalid_argument(
            std::format("the wave '{}' needs a positive spawn interval", definition.name));
    }
    if (definition.placement == WavePlacement::Manual) {
        checkManual(definition, enemies);
    } else if (definition.placement == WavePlacement::Music) {
        checkMusic(definition, enemies);
    } else {
        checkRandom(definition, enemies);
    }
}

} // namespace

void WaveRegistry::addDefinition(WaveDefinition definition, const EnemyRegistry &enemies)
{
    checkDefinition(definition, enemies);
    if (_definitions.contains(definition.name)) {
        throw std::invalid_argument(std::format("the wave '{}' is already defined", definition.name));
    }
    const std::string name = definition.name;
    _definitions.emplace(name, std::move(definition));
}

bool WaveRegistry::hasDefinition(const std::string &name) const
{
    return _definitions.contains(name);
}

const WaveDefinition &WaveRegistry::getDefinition(const std::string &name) const
{
    const auto found = _definitions.find(name);
    if (found == _definitions.end()) {
        throw std::out_of_range(std::format("unknown wave '{}'", name));
    }
    return found->second;
}

std::vector<std::string> WaveRegistry::listWavesOfDifficulty(std::uint8_t difficulty) const
{
    std::vector<std::string> names;
    for (const auto &[name, definition] : _definitions) {
        if (definition.difficulty == difficulty) {
            names.push_back(name);
        }
    }
    std::ranges::sort(names);
    return names;
}

} // namespace rtype::game
