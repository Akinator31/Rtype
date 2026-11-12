#include "rtype/game/System/WaveSystem.hpp"

#include "rtype/game/Entities.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

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
    // During the rescue against the final boss, its RescueSystem ends the game
    if (_context.over || _context.rescue.stage != RescueStage::None || _context.waves.isEmpty() ||
        !hasPlayers()) {
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
    spawnDueItems();
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
    if (_context.rules.endless) {
        const std::vector<std::string> names = _context.waves.listAllWaves();
        const auto last = static_cast<std::uint32_t>(names.size() - 1);
        ++progress.number;
        startWave(names[_context.randomBetween(0, last)]);
        return;
    }
    while (progress.number < MaxWaveDifficulty) {
        ++progress.number;
        const std::vector<std::string> names =
            _context.waves.listWavesOfDifficulty(static_cast<std::uint8_t>(progress.number));
        if (names.empty()) {
            continue;
        }
        const auto last = static_cast<std::uint32_t>(names.size() - 1);
        startWave(names[_context.randomBetween(0, last)]);
        return;
    }
    _context.over = true;
    _context.gameOver = GameOutcome::Victory;
}

void WaveSystem::startWave(std::string name)
{
    WaveProgress &progress = _context.wave;
    progress.name = std::move(name);
    progress.spawned = 0;
    progress.nextSpawnIn = 0;
    progress.elapsed = 0;
    _context.waveStarted = progress.number;
    _context.waveMusic = _context.waves.getDefinition(progress.name).music;
    scheduleItems(_context.waves.getDefinition(progress.name));
    spawnDueEnemies(_context.waves.getDefinition(progress.name));
}

void WaveSystem::spawnTimelineEnemies(const WaveDefinition &wave)
{
    WaveProgress &progress = _context.wave;
    while (progress.spawned < wave.quantity && wave.timeline[progress.spawned].tick <= progress.elapsed) {
        spawnAt(wave.timeline[progress.spawned].spawn);
        ++progress.spawned;
    }
    ++progress.elapsed;
}

void WaveSystem::spawnAt(const WaveSpawn &spawn)
{
    _ecs.defer([this, spawn] { spawnEnemy(_ecs, _context, spawn.enemy, spawn.x, spawn.y); });
}

void WaveSystem::spawnDueEnemies(const WaveDefinition &wave)
{
    if (wave.placement == WavePlacement::Music) {
        spawnTimelineEnemies(wave);
        return;
    }
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
    spawnAt(spawn);
}

std::uint32_t WaveSystem::findItemWindow(const WaveDefinition &wave) const
{
    std::uint32_t window = 0;
    if (wave.placement == WavePlacement::Music) {
        window = wave.timeline.empty() ? 0 : wave.timeline.back().tick;
    } else if (wave.quantity > 1) {
        window = toTicks(wave.spawnInterval) * static_cast<std::uint32_t>(wave.quantity - 1);
    }
    return std::max(window, _context.rules.itemMinWindow);
}

void WaveSystem::scheduleItems(const WaveDefinition &wave)
{
    WaveProgress &progress = _context.wave;
    progress.items.clear();
    progress.itemTicks = 0;
    const std::uint32_t window = findItemWindow(wave);
    for (const WaveItem &item : wave.items) {
        const std::uint32_t count = item.maxQuantity > item.quantity
                                        ? _context.randomBetween(item.quantity, item.maxQuantity)
                                        : item.quantity;
        for (std::uint32_t i = 0; i < count; ++i) {
            progress.items.push_back({_context.randomBetween(0, window), item.item});
        }
    }
    std::ranges::stable_sort(progress.items, {}, &ScheduledItem::tick);
}

void WaveSystem::spawnDueItems()
{
    WaveProgress &progress = _context.wave;
    std::size_t due = 0;
    while (due < progress.items.size() && progress.items[due].tick <= progress.itemTicks) {
        spawnItemAtRandom(progress.items[due].item);
        ++due;
    }
    progress.items.erase(progress.items.begin(), progress.items.begin() + static_cast<std::ptrdiff_t>(due));
    ++progress.itemTicks;
}

void WaveSystem::spawnItemAtRandom(const std::string &item)
{
    const Rules &rules = _context.rules;
    const float x = _context.randomNumber(rules.itemMinX, GameWidth - rules.itemMargin);
    const float y = _context.randomNumber(rules.itemMargin, GameHeight - rules.itemMargin);
    _ecs.defer([this, item, x, y] { spawnItem(_ecs, _context, item, x, y); });
}

void WaveSystem::clearWave()
{
    for (PlayerSlot &player : _context.players) {
        if (!player.present) {
            continue;
        }
        if (!player.isAlive()) {
            player.streak = 0;
            continue;
        }
        ++player.wavesCleared;
        ++player.streak;
        player.bestStreak = std::max(player.bestStreak, player.streak);
        if (_context.rules.endless) {
            const Progression &progression = _context.progression;
            player.xp += applyMultiplier(progression.xpPerWave, streakMultiplier(progression, player.streak));
        }
    }
}

void WaveSystem::endWave()
{
    WaveProgress &progress = _context.wave;
    progress.name.clear();
    // The items of a wave cleared before their time come all the same
    for (const ScheduledItem &item : progress.items) {
        spawnItemAtRandom(item.item);
    }
    progress.items.clear();
    clearWave();
    if (_context.rules.endless || progress.number < MaxWaveDifficulty) {
        progress.pauseLeft = _context.rules.wavePause;
        return;
    }
    _context.over = true;
    _context.gameOver = GameOutcome::Victory;
}

} // namespace rtype::game
