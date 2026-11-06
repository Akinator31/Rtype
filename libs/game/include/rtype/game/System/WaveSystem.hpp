#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"

namespace rtype::game
{

/// Runs the waves of a game (context.wave). Wave n is one of the waves of difficulty n, drawn at random,
/// from MinWaveDifficulty to MaxWaveDifficulty; a difficulty without a wave is skipped. A wave sends its
/// enemies one every spawnInterval seconds, where its placement says, and ends once they are all sent and
/// none is left (destroyed or gone out of the area). A Music wave sends each at the tick its timeline says,
/// on the beats of its music, instead of at regular intervals. The next one starts Rules::wavePause ticks later; the
/// first one Rules::firstWaveDelay ticks after the game starts. Once the last one ends, the game is won
/// (GameOutcome::Victory).
///
/// Nothing happens without a wave in context.waves, nor while no player is in the game, nor during the
/// rescue against the final boss (RescueSystem), which ends the game itself. The enemies are created after
/// the systems of the tick.
class WaveSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<Enemy> &_enemies;

    [[nodiscard]] bool hasPlayers() const;
    [[nodiscard]] bool hasEnemiesLeft() const;
    void startNextWave();
    void spawnDueEnemies(const WaveDefinition &wave);
    void spawnTimelineEnemies(const WaveDefinition &wave);
    void spawnAt(const WaveSpawn &spawn);
    void spawnEnemyOf(const WaveDefinition &wave, std::uint16_t index);
    void endWave();

public:
    WaveSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
