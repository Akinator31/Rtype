// Tests of the rescue against the final boss, offline (Rules::rescue, RescueSystem): the shogun of the
// configuration of the repository, the last life lost against it, the allies, the dash, the combined attack.

#include "rtype/game/Game.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

using namespace rtype::game;

namespace
{

/// The configuration of the repository with its final boss only.
GameConfig bossOnly()
{
    const LoadedGameConfig loaded = loadGameConfig(RTYPE_TEST_CONFIG);
    REQUIRE(loaded.isValid());
    GameConfig config = loaded.config;
    std::erase_if(config.waves,
                  [](const WaveDefinition &wave) { return wave.difficulty != MaxWaveDifficulty; });
    REQUIRE_FALSE(config.waves.empty());
    return config;
}

std::size_t countOf(const Snapshot &snapshot, EntityKind kind)
{
    return static_cast<std::size_t>(std::ranges::count(snapshot.entities, kind, &EntityState::kind));
}

std::optional<EntityState> bossOf(const Snapshot &snapshot)
{
    const auto boss = std::ranges::find(snapshot.entities, EntityKind::BydoBoss, &EntityState::kind);
    return boss == snapshot.entities.end() ? std::nullopt : std::optional<EntityState>(*boss);
}

/// A game against the shogun with one life, which the player loses by flying into it.
std::unique_ptr<Simulation> loseAgainstTheBoss(bool rescue)
{
    Rules rules;
    rules.rescue = rescue;
    rules.lives = 1;
    rules.firstWaveDelay = 1;
    auto simulation = std::make_unique<Simulation>(rules, bossOnly());
    simulation->addPlayer(0);
    // It comes in from the right of the game area, where the ship cannot go
    for (int tick = 0; tick < 5 * TickRate; ++tick) {
        simulation->step();
        const std::optional<EntityState> boss = bossOf(simulation->snapshot());
        if (boss && boss->x < 1700.0f) {
            break;
        }
    }
    REQUIRE(bossOf(simulation->snapshot()));
    REQUIRE(simulation->context().wave.number == MaxWaveDifficulty);

    rtype::ecs::Ecs &ecs = simulation->ecs();
    const auto ship = findPlayerShip(ecs, 0);
    REQUIRE(ship);
    const EntityState boss = *bossOf(simulation->snapshot());
    ecs.getComponent<rtype::ecs::Position>(*ship) = {boss.x, boss.y};
    ecs.getComponent<Health>(*ship).points = 1;
    simulation->step();
    REQUIRE_FALSE(findPlayerShip(ecs, 0));
    return simulation;
}

/// Steps until the rescue reaches a stage, at most `ticks` times: the game over of the ticks stepped, if any.
std::optional<GameOutcome> runUntil(Simulation &simulation, RescueStage stage, int ticks)
{
    std::optional<GameOutcome> outcome;
    for (int tick = 0; tick < ticks && simulation.snapshot().rescue != stage; ++tick) {
        simulation.step();
        outcome = outcome ? outcome : simulation.snapshot().gameOver;
    }
    return outcome;
}

} // namespace

TEST_CASE("Online, the last life lost against the final boss is the defeat", "[game]")
{
    const std::unique_ptr<Simulation> simulation = loseAgainstTheBoss(false);
    CHECK(simulation->snapshot().gameOver == GameOutcome::Defeat);
    CHECK(simulation->snapshot().rescue == RescueStage::None);
}

TEST_CASE("Offline, the last life lost against the final boss calls the allies, and the combined attack wins",
          "[game]")
{
    const std::unique_ptr<Simulation> simulation = loseAgainstTheBoss(true);
    Simulation &game = *simulation;
    const Rules &rules = game.rules();

    // No defeat: the voice plays, the boss waits
    CHECK_FALSE(game.snapshot().gameOver);
    CHECK(game.snapshot().rescue == RescueStage::Voice);
    CHECK(game.snapshot().players.at(0).lives == 0);
    CHECK_FALSE(runUntil(game, RescueStage::Arrival, static_cast<int>(rules.rescueVoiceTicks) + 1));
    REQUIRE(game.snapshot().rescue == RescueStage::Arrival);

    // Five allies and a new ship, with a life back
    game.step();
    CHECK(countOf(game.snapshot(), EntityKind::AllyShip) == 5);
    CHECK(countOf(game.snapshot(), EntityKind::PlayerShip) == 1);
    CHECK(game.snapshot().players.at(0).lives == 1);
    const std::uint8_t health = bossOf(game.snapshot())->health;

    // The allies shoot at the boss, which cannot be hurt
    CHECK_FALSE(runUntil(game, RescueStage::Assault, static_cast<int>(rules.rescueArrivalTicks) + 1));
    std::size_t shots = 0;
    for (std::uint32_t tick = 0;
         tick < rules.rescueAssaultTicks && game.snapshot().rescue == RescueStage::Assault;
         ++tick) {
        game.step();
        shots = std::max(shots, countOf(game.snapshot(), EntityKind::PlayerShot));
    }
    CHECK(shots > 0);
    CHECK(bossOf(game.snapshot())->health == health);

    // It dashes at every ship, slashing, strikes each one once, and kills none
    REQUIRE(game.snapshot().rescue == RescueStage::Dash);
    bool slashed = false;
    float leftmost = 2000.0f;
    std::vector<std::uint32_t> struck;
    for (int tick = 0; tick < 30 * TickRate && game.snapshot().rescue == RescueStage::Dash; ++tick) {
        game.step();
        for (const std::uint32_t id : game.snapshot().struck) {
            const auto ship = std::ranges::find(game.snapshot().entities, id, &EntityState::id);
            REQUIRE(ship != game.snapshot().entities.end());
            CHECK((ship->kind == EntityKind::PlayerShip || ship->kind == EntityKind::AllyShip));
            struck.push_back(id);
        }
        const EntityState boss = *bossOf(game.snapshot());
        slashed = slashed || (boss.flags & EntityState::Slashing) != 0;
        leftmost = std::min(leftmost, boss.x);
        REQUIRE(countOf(game.snapshot(), EntityKind::AllyShip) == 5);
        REQUIRE(countOf(game.snapshot(), EntityKind::PlayerShip) == 1);
    }
    REQUIRE(game.snapshot().rescue == RescueStage::Prompt);
    CHECK(slashed);
    CHECK(leftmost < 300.0f);
    CHECK(struck.size() == 6);
    std::ranges::sort(struck);
    CHECK(std::ranges::adjacent_find(struck) == struck.end());
    CHECK(countOf(game.snapshot(), EntityKind::AllyShip) == 5);
    CHECK(countOf(game.snapshot(), EntityKind::PlayerShip) == 1);
    CHECK(game.snapshot().players.at(0).lives == 1);

    // Fire held from before does not count: released, then pressed
    PlayerInput fire;
    fire.set(PlayerInput::Fire, true);
    game.setInput(0, fire);
    for (int tick = 0; tick < 30; ++tick) {
        game.step();
    }
    CHECK(game.snapshot().rescue == RescueStage::Prompt);
    game.setInput(0, {});
    game.step();
    game.setInput(0, fire);
    game.step();
    REQUIRE(game.snapshot().rescue == RescueStage::Combined);

    // The boss falls, scored by the player, then the victory
    const std::uint32_t score = game.snapshot().players.at(0).score;
    CHECK_FALSE(runUntil(game, RescueStage::Finished, static_cast<int>(rules.rescueCombinedTicks) + 2));
    REQUIRE(game.snapshot().rescue == RescueStage::Finished);
    game.step();
    CHECK_FALSE(bossOf(game.snapshot()));
    CHECK(game.snapshot().players.at(0).score >= score + 10000);
    std::optional<GameOutcome> outcome;
    for (std::uint32_t tick = 0; tick <= rules.rescueVictoryDelay && !outcome; ++tick) {
        game.step();
        outcome = game.snapshot().gameOver;
    }
    CHECK(outcome == GameOutcome::Victory);
}
