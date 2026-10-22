#include "rtype/game/System/WaveSystem.hpp"

#include "rtype/game/Entities.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::game
{

namespace
{

std::uint32_t toTicks(float seconds)
{
    return static_cast<std::uint32_t>(std::lround(seconds * static_cast<float>(TickRate)));
}

} // namespace

WaveSystem::WaveSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context)
    : _ecs(ecs), _context(context), _enemies(use<Enemy>(caches))
{
    _context.wave.pauseLeft = _context.rules.firstWaveDelay;
}

void WaveSystem::update(float)
{
    if (_context.over || _context.waves.isEmpty() || !hasPlayers()) {
        return;
    }
    WaveProgress &progress = _context.wave;
    if (progress.name.empty()) {
        if (progress.pauseLeft > 0 && --progress.pauseLeft > 0) {
            return;
        }
        startNextWave();
        return;
    }
    const WaveDefinition &wave = _context.waves.getDefinition(progress.name);
    // The enemies sent are created after the systems: they count from the next tick
    if (progress.spawned == wave.quantity && !hasEnemiesLeft()) {
        endWave();
        return;
    }
    spawnDueEnemies(wave);
}

bool WaveSystem::hasPlayers() const
{
    return std::ranges::any_of(_context.players, [](const PlayerSlot &player) { return player.present; });
}

bool WaveSystem::hasEnemiesLeft() const
{
    for (ecs::entity_t entity = 0; entity < _enemies.size(); ++entity) {
        if (_enemies.has(entity) && _ecs.isAlive(entity)) {
            return true;
        }
    }
    return false;
}

void WaveSystem::startNextWave()
{
    WaveProgress &progress = _context.wave;
    while (progress.number < MaxWaveDifficulty) {
        ++progress.number;
        const std::vector<std::string> names = _context.waves.listWavesOfDifficulty(progress.number);
        if (names.empty()) {
            continue;
        }
        const auto last = static_cast<std::uint32_t>(names.size() - 1);
        progress.name = names[_context.randomBetween(0, last)];
        progress.spawned = 0;
        progress.nextSpawnIn = 0;
        _context.waveStarted = progress.number;
        spawnDueEnemies(_context.waves.getDefinition(progress.name));
        return;
    }
    _context.over = true;
    _context.gameOver = GameOutcome::Victory;
}

void WaveSystem::spawnDueEnemies(const WaveDefinition &wave)
{
    WaveProgress &progress = _context.wave;
    if (progress.nextSpawnIn > 0) {
        --progress.nextSpawnIn;
    }
    while (progress.nextSpawnIn == 0 && progress.spawned < wave.quantity) {
        spawnEnemyOf(wave, progress.spawned);
        ++progress.spawned;
        progress.nextSpawnIn = toTicks(wave.spawnInterval);
    }
}

void WaveSystem::spawnEnemyOf(const WaveDefinition &wave, std::uint16_t index)
{
    WaveSpawn spawn;
    if (wave.placement == WavePlacement::Manual) {
        spawn = wave.spawns[index];
    } else {
        const WaveArea &area = wave.area;
        spawn = {wave.enemy, _context.randomNumber(area.minX, area.maxX),
                 _context.randomNumber(area.minY, area.maxY)};
    }
    _ecs.defer([this, spawn] { spawnEnemy(_ecs, _context, spawn.enemy, spawn.x, spawn.y); });
}

void WaveSystem::endWave()
{
    WaveProgress &progress = _context.wave;
    progress.name.clear();
    if (progress.number < MaxWaveDifficulty) {
        progress.pauseLeft = _context.rules.wavePause;
        return;
    }
    _context.over = true;
    _context.gameOver = GameOutcome::Victory;
}

} // namespace rtype::game
