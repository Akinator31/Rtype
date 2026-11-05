#include "Commands/BossCommand.hpp"

#include "Game/LocalSession.hpp"
#include "rtype/game/Rules.hpp"
#include "rtype/game/Wave/WaveDefinition.hpp"

#include <algorithm>
#include <memory>

namespace rtype::client
{

namespace
{

/// Ticks before the boss wave starts: a second, instead of the pause before the first wave.
constexpr std::uint32_t BossDelay = 60;

void startBossGame(GameScenes &games, Console &console, const game::GameConfig &config)
{
    game::GameConfig bossOnly = config;
    std::erase_if(bossOnly.waves, [](const game::WaveDefinition &wave) {
        return wave.difficulty != game::MaxWaveDifficulty;
    });
    if (bossOnly.waves.empty()) {
        console.print("No wave of difficulty 6 in config/waves/");
        return;
    }
    game::Rules rules;
    rules.firstWaveDelay = BossDelay;
    games.play(std::make_unique<LocalSession>(rules, bossOnly));
}

} // namespace

void registerBossCommand(Console &console, GameScenes &games, const game::GameConfig &config)
{
    console.registerConsoleCommand("boss", "boss: starts a solo game against the final boss only",
                                   [&games, &config](Console &target, const Console::Arguments &) {
                                       startBossGame(games, target, config);
                                   });
}

} // namespace rtype::client
