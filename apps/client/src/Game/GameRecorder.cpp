#include "Game/GameRecorder.hpp"

#include "Settings/PlayerSettings.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace rtype::client
{

namespace
{

using KindStat = std::pair<game::EntityKind, const char *>;

/// The stat of each kind of Bydo counted apart.
constexpr std::array<KindStat, 3> BydoKindStats{{
    {game::EntityKind::BydoFlyer, FlyersKilledStat},
    {game::EntityKind::BydoShooter, ShootersKilledStat},
    {game::EntityKind::BydoTank, TanksKilledStat},
}};

constexpr std::array<const char *, 8> GameEvents{GameStartedEvent, BydoKilledEvent, BossKilledEvent,
                                                 PlayerKilledEvent, WaveStartedEvent, GameWonEvent,
                                                 GameLostEvent,     GameEndedEvent};

int toStat(std::uint32_t value)
{
    return static_cast<int>(std::min<std::uint32_t>(value, std::numeric_limits<int>::max()));
}

} // namespace

int getStat(const engine::Settings &settings, const char *stat)
{
    return settings.getInt(stat).value_or(0);
}

GameRecorder::GameRecorder(engine::Engine &engine) : _engine(engine)
{
    for (const char *event : GameEvents) {
        _engine.createEvent(event);
    }
}

void GameRecorder::addToStat(const char *stat, int amount)
{
    engine::Settings &settings = _engine.getSettings();
    const int current = getStat(settings, stat);
    settings.setInt(stat, current > std::numeric_limits<int>::max() - amount ? current : current + amount);
}

void GameRecorder::raiseStat(const char *stat, int value)
{
    engine::Settings &settings = _engine.getSettings();
    settings.setInt(stat, std::max(getStat(settings, stat), value));
}

void GameRecorder::recordStart()
{
    addToStat(GamesPlayedStat, 1);
    _engine.triggerEvent(GameStartedEvent);
}

void GameRecorder::recordKill(const Kill &kill, std::uint8_t localSlot)
{
    if (kill.kind == game::EntityKind::PlayerShip) {
        if (kill.owner == localSlot) {
            addToStat(DeathsStat, 1);
            _engine.triggerEvent(PlayerKilledEvent);
        }
        return;
    }
    if (kill.kind == game::EntityKind::BydoBoss) {
        addToStat(BossesDefeatedStat, 1);
        _engine.triggerEvent(BossKilledEvent);
    }
    if (kill.killer != localSlot) {
        return;
    }
    addToStat(BydosKilledStat, 1);
    const auto kindStat = std::ranges::find(BydoKindStats, kill.kind, &KindStat::first);
    if (kindStat != BydoKindStats.end()) {
        addToStat(kindStat->second, 1);
    }
    _engine.triggerEvent(BydoKilledEvent);
}

void GameRecorder::recordWave(std::uint16_t wave)
{
    raiseStat(BestWaveStat, wave);
    _engine.triggerEvent(WaveStartedEvent);
}

void GameRecorder::recordOutcome(game::GameOutcome outcome)
{
    if (outcome == game::GameOutcome::Victory) {
        addToStat(VictoriesStat, 1);
        _engine.triggerEvent(GameWonEvent);
        return;
    }
    addToStat(DefeatsStat, 1);
    _engine.triggerEvent(GameLostEvent);
}

void GameRecorder::recordEnd(std::uint32_t score)
{
    raiseStat(BestScoreStat, toStat(score));
    addToStat(TotalScoreStat, toStat(score));
    writeSettings(_engine.getSettings());
    _engine.triggerEvent(GameEndedEvent);
}

} // namespace rtype::client
