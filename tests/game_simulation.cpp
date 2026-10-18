// Tests of libs/game: a Simulation stepped tick by tick, given inputs, and checked through its snapshots
// (ADR 0015, docs/architecture/game.md).

#include "rtype/game/Game.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using namespace rtype::game;
using Catch::Approx;

namespace
{

PlayerInput held(std::uint8_t buttons)
{
    return PlayerInput{buttons};
}

void run(Simulation &simulation, int ticks)
{
    for (int i = 0; i < ticks; ++i) {
        simulation.step();
    }
}

std::optional<EntityState> shipOf(const Snapshot &snapshot, std::uint8_t slot)
{
    for (const EntityState &entity : snapshot.entities) {
        if (entity.kind == EntityKind::PlayerShip && entity.owner == slot) {
            return entity;
        }
    }
    return std::nullopt;
}

std::vector<EntityState> shotsOf(const Snapshot &snapshot)
{
    std::vector<EntityState> shots;
    std::ranges::copy_if(snapshot.entities, std::back_inserter(shots),
                         [](const EntityState &entity) { return entity.kind == EntityKind::PlayerShot; });
    return shots;
}

} // namespace

// --- Inputs ---

TEST_CASE("An input gives the direction of its buttons, opposite ones cancelling", "[game]")
{
    CHECK(held(PlayerInput::Up).direction().y == -1);
    CHECK(held(PlayerInput::Down).direction().y == 1);
    CHECK(held(PlayerInput::Left).direction().x == -1);
    CHECK(held(PlayerInput::Right).direction().x == 1);
    CHECK(held(PlayerInput::Up | PlayerInput::Down).direction().y == 0);
    CHECK(held(PlayerInput::Left | PlayerInput::Right).direction().x == 0);

    PlayerInput input;
    input.set(PlayerInput::Fire, true);
    input.set(PlayerInput::Up, true);
    input.set(PlayerInput::Up, false);
    CHECK(input.buttons == PlayerInput::Fire);
    CHECK(PlayerInput::fromByte(0xFF).buttons == 0x1F);
}

// --- Players ---

TEST_CASE("A new simulation is empty, and counts its ticks from 1", "[game]")
{
    Simulation simulation;
    CHECK(simulation.tick() == 0);

    simulation.step();

    CHECK(simulation.tick() == 1);
    CHECK(simulation.snapshot().tick == 1);
    CHECK(simulation.snapshot().players.empty());
    CHECK(simulation.snapshot().entities.empty());
}

TEST_CASE("A player gets a ship in the game area, with the lives of the rules", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(2);
    simulation.step();

    const Snapshot &snapshot = simulation.snapshot();
    REQUIRE(snapshot.players.size() == 1);
    CHECK(snapshot.players[0] == PlayerState{2, simulation.rules().lives, 0});
    const auto ship = shipOf(snapshot, 2);
    REQUIRE(ship);
    CHECK(ship->id != 0);
    CHECK(ship->x > 0.0f);
    CHECK(ship->x < GameWidth);
    CHECK(ship->y > 0.0f);
    CHECK(ship->y < GameHeight);
    CHECK(ship->health == simulation.rules().shipHealth);
    CHECK(simulation.hasPlayer(2));
    CHECK(simulation.playerCount() == 1);
}

TEST_CASE("Four players get four ships at different heights", "[game]")
{
    Simulation simulation;
    for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
        simulation.addPlayer(slot);
    }
    simulation.step();

    std::vector<float> heights;
    for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
        const auto ship = shipOf(simulation.snapshot(), slot);
        REQUIRE(ship);
        heights.push_back(ship->y);
    }
    CHECK(std::ranges::is_sorted(heights));
    CHECK(std::ranges::adjacent_find(heights) == heights.end());
}

TEST_CASE("A slot is 0 to 3, and taken once", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);

    CHECK_THROWS_AS(simulation.addPlayer(0), std::invalid_argument);
    CHECK_THROWS_AS(simulation.addPlayer(4), std::invalid_argument);
    CHECK(simulation.playerCount() == 1);
}

TEST_CASE("A player who leaves takes its ship away, and the snapshot says so", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(1);
    simulation.step();
    const std::uint32_t id = shipOf(simulation.snapshot(), 1)->id;

    simulation.removePlayer(1);
    simulation.step();

    const Snapshot &snapshot = simulation.snapshot();
    CHECK(snapshot.players.empty());
    CHECK_FALSE(shipOf(snapshot, 1));
    CHECK(snapshot.destroyed == std::vector<DestroyedEntity>{{id, DestroyCause::Removed}});
    CHECK_FALSE(simulation.hasPlayer(1));
    // Reported once
    simulation.step();
    CHECK(simulation.snapshot().destroyed.empty());
    // An empty slot can be left again, and taken again
    simulation.removePlayer(1);
    simulation.addPlayer(1);
}

// --- Movement ---

TEST_CASE("A ship moves with the input of its player, at the speed of the rules", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    simulation.step();
    const EntityState start = *shipOf(simulation.snapshot(), 0);

    simulation.setInput(0, held(PlayerInput::Right));
    run(simulation, 30);

    const EntityState moved = *shipOf(simulation.snapshot(), 0);
    CHECK(moved.x == Approx(start.x + simulation.rules().shipSpeed * 30 * TickDuration));
    CHECK(moved.y == Approx(start.y));
    CHECK(moved.velocityX == Approx(simulation.rules().shipSpeed));

    // The input stays until the next one
    simulation.setInput(0, {});
    run(simulation, 10);
    CHECK(shipOf(simulation.snapshot(), 0)->x == Approx(moved.x));
    CHECK(shipOf(simulation.snapshot(), 0)->velocityX == 0.0f);
}

TEST_CASE("A ship goes as fast in diagonal as in a straight line", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    simulation.setInput(0, held(PlayerInput::Down | PlayerInput::Right));
    simulation.step();

    const EntityState ship = *shipOf(simulation.snapshot(), 0);
    CHECK(std::hypot(ship.velocityX, ship.velocityY) == Approx(simulation.rules().shipSpeed));
}

TEST_CASE("A ship stays in the game area", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    const Rules &rules = simulation.rules();

    simulation.setInput(0, held(PlayerInput::Up | PlayerInput::Left));
    run(simulation, 600);
    EntityState ship = *shipOf(simulation.snapshot(), 0);
    CHECK(ship.x == Approx(rules.shipHalfWidth));
    CHECK(ship.y == Approx(rules.shipHalfHeight));

    simulation.setInput(0, held(PlayerInput::Down | PlayerInput::Right));
    run(simulation, 600);
    ship = *shipOf(simulation.snapshot(), 0);
    CHECK(ship.x == Approx(GameWidth - rules.shipHalfWidth));
    CHECK(ship.y == Approx(GameHeight - rules.shipHalfHeight));
}

TEST_CASE("The input of an empty slot is ignored", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    simulation.setInput(3, held(PlayerInput::Fire));
    run(simulation, 30);

    CHECK(shotsOf(simulation.snapshot()).empty());
}

// --- Shots ---

TEST_CASE("Holding Fire shoots at the interval of the rules, from the nose of the ship", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    simulation.setInput(0, held(PlayerInput::Fire));

    simulation.step();
    const std::vector<EntityState> first = shotsOf(simulation.snapshot());
    REQUIRE(first.size() == 1);
    const EntityState ship = *shipOf(simulation.snapshot(), 0);
    CHECK(first[0].owner == 0);
    CHECK(first[0].x > ship.x);
    CHECK(first[0].y == Approx(ship.y));
    CHECK(first[0].velocityX == Approx(simulation.rules().shotSpeed));

    run(simulation, static_cast<int>(simulation.rules().fireInterval) - 1);
    CHECK(shotsOf(simulation.snapshot()).size() == 1);
    simulation.step();
    CHECK(shotsOf(simulation.snapshot()).size() == 2);
}

TEST_CASE("A shot that leaves the area is removed, and its id is never given again", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    simulation.setInput(0, held(PlayerInput::Fire));
    simulation.step();
    const std::uint32_t firstShot = shotsOf(simulation.snapshot())[0].id;
    simulation.setInput(0, {});

    // 1500 units per second: across the area in less than 2 seconds
    std::optional<std::uint32_t> removedAt;
    for (int tick = 0; tick < 180 && !removedAt; ++tick) {
        simulation.step();
        const auto &destroyed = simulation.snapshot().destroyed;
        if (std::ranges::find(destroyed, DestroyedEntity{firstShot, DestroyCause::Removed}) !=
            destroyed.end()) {
            removedAt = simulation.tick();
        }
    }
    REQUIRE(removedAt);
    CHECK(shotsOf(simulation.snapshot()).empty());

    simulation.setInput(0, held(PlayerInput::Fire));
    simulation.step();
    const std::vector<EntityState> shots = shotsOf(simulation.snapshot());
    REQUIRE(shots.size() == 1);
    CHECK(shots[0].id > firstShot);
}

// --- The whole game ---

TEST_CASE("The same inputs give exactly the same game", "[game]")
{
    Simulation first;
    Simulation second;
    std::mt19937 random(2026);
    std::uniform_int_distribution<int> buttons(0, 0x1F);
    for (std::uint8_t slot = 0; slot < 3; ++slot) {
        first.addPlayer(slot);
        second.addPlayer(slot);
    }

    for (int tick = 0; tick < 1200; ++tick) {
        if (tick % 15 == 0) {
            for (std::uint8_t slot = 0; slot < 3; ++slot) {
                const PlayerInput input{static_cast<std::uint8_t>(buttons(random))};
                first.setInput(slot, input);
                second.setInput(slot, input);
            }
        }
        first.step();
        second.step();
        REQUIRE(first.snapshot() == second.snapshot());
    }
    CHECK_FALSE(first.snapshot().entities.empty());
}

TEST_CASE("Every entity stays where the protocol can encode it, and ids are unique", "[game]")
{
    Simulation simulation;
    std::mt19937 random(7);
    std::uniform_int_distribution<int> buttons(0, 0x1F);
    for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
        simulation.addPlayer(slot);
    }

    for (int tick = 0; tick < 3600; ++tick) {
        if (tick % 10 == 0) {
            for (std::uint8_t slot = 0; slot < MaxPlayers; ++slot) {
                simulation.setInput(slot, PlayerInput{static_cast<std::uint8_t>(buttons(random))});
            }
        }
        simulation.step();
        const Snapshot &snapshot = simulation.snapshot();
        for (std::size_t i = 0; i < snapshot.entities.size(); ++i) {
            const EntityState &entity = snapshot.entities[i];
            REQUIRE(std::abs(entity.x) < PositionLimit);
            REQUIRE(std::abs(entity.y) < PositionLimit);
            REQUIRE(entity.id != 0);
            // Sorted by id, so equal ids would be next to each other
            REQUIRE((i == 0 || snapshot.entities[i - 1].id < entity.id));
        }
    }
}
