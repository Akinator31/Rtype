// The characteristics of the ships of the hangar: read from config/hangar.lua, shown as gauges, applied by
// the game to the speed, the shots and the health of the ship of the player, predicted by its client, and
// sent over the network.

#include "Game/Prediction.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/game/Game.hpp"
#include "rtype/game/Item/ItemEffects.hpp"
#include "rtype/game/Messages.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace rtype::game;

namespace
{

/// A cannon of 10 damage, and two ships of the hangar: a fast and frail one, a slow and strong one.
GameConfig statsConfig()
{
    GameConfig config;
    config.projectiles.push_back(
        {"shot", "", ProjectileCategory::Bullet, {rtype::physics::Aabb{{}, {12.0f, 4.0f, 0.0f}}, {}, false}});
    config.weapons.push_back(
        {"cannon", "shot", ProjectileCategory::Bullet, 1500.0f, 10, 0.0f, InfiniteAmmo, 0, 10});
    config.ships.push_back({"fighter", "models/fighter.glb", {}});
    config.player = {"fighter", "cannon", 100};
    config.shipUnlocks = {{"VX-01", 1, {1.0f, 0.0f, 0.0f}}, {"VX-03", 2, {0.0f, 1.0f, 1.0f}}};
    return config;
}

std::uint8_t healthOf(Simulation &simulation, std::uint8_t slot)
{
    return simulation.ecs().getComponent<Health>(*findPlayerShip(simulation.ecs(), slot)).points;
}

rtype::ecs::Position positionOf(Simulation &simulation, std::uint8_t slot)
{
    return simulation.ecs().getComponent<rtype::ecs::Position>(*findPlayerShip(simulation.ecs(), slot));
}

PlayerInput holding(PlayerInput::Button button)
{
    PlayerInput input;
    input.set(button, true);
    return input;
}

} // namespace

TEST_CASE("A characteristic multiplies by 0.75 at 0, 1 at 0.5 and 1.25 at 1", "[game]")
{
    CHECK(statFactor(0.0f) == 0.75f);
    CHECK(statFactor(0.5f) == 1.0f);
    CHECK(statFactor(1.0f) == 1.25f);
    CHECK(scaleByStat(100, 0.0f) == 75);
    CHECK(scaleByStat(100, 0.5f) == 100);
    CHECK(scaleByStat(100, 1.0f) == 125);
    CHECK(scaleByStat(10, 0.3f) == 9);
    // From 1 to 255
    CHECK(scaleByStat(1, 0.0f) == 1);
    CHECK(scaleByStat(250, 1.0f) == 255);
}

TEST_CASE("A player flies a ship of the hangar, or the ship of the rules", "[game]")
{
    Simulation simulation({}, statsConfig());
    CHECK_FALSE(simulation.setShip(0, "VX-01"));
    simulation.addPlayer(0);
    CHECK(simulation.statsOf(0) == ShipStats{});
    CHECK_FALSE(simulation.setShip(0, "VX-99"));
    CHECK(simulation.statsOf(0) == ShipStats{});

    REQUIRE(simulation.setShip(0, "VX-01"));
    CHECK(simulation.statsOf(0) == ShipStats{1.0f, 0.0f, 0.0f});
    REQUIRE(simulation.setShip(0, ""));
    CHECK(simulation.statsOf(0) == ShipStats{});
}

TEST_CASE("The armor of the ship makes its health, and caps what heals it", "[game]")
{
    Simulation simulation({}, statsConfig());
    simulation.addPlayer(0);
    CHECK(healthOf(simulation, 0) == 100);

    // A ship not hit yet gets all its new health at once
    REQUIRE(simulation.setShip(0, "VX-03"));
    CHECK(healthOf(simulation, 0) == 125);
    REQUIRE(simulation.setShip(0, "VX-01"));
    CHECK(healthOf(simulation, 0) == 75);

    // A ship hit keeps what it has left, within its new health
    const rtype::ecs::entity_t ship = *findPlayerShip(simulation.ecs(), 0);
    simulation.ecs().getComponent<Health>(ship).points = 50;
    REQUIRE(simulation.setShip(0, "VX-03"));
    CHECK(healthOf(simulation, 0) == 50);

    // Healed up to it, and every new ship has it
    CHECK(applyItemEffect(simulation.ecs(), simulation.context(), ship, {ItemEffectType::Heal, 200}));
    CHECK(healthOf(simulation, 0) == 125);
    destroyEntity(simulation.ecs(), simulation.context(), ship, DestroyCause::Killed);
    spawnPlayerShip(simulation.ecs(), simulation.context(), 0);
    CHECK(healthOf(simulation, 0) == 125);
}

TEST_CASE("The power of the ship makes the damage of its shots, not of those of the Bydos", "[game]")
{
    Simulation simulation({}, statsConfig());
    simulation.addPlayer(0);
    const auto damageOf = [&simulation](std::uint8_t owner) {
        const rtype::ecs::entity_t shot =
            spawnWeaponShot(simulation.ecs(), simulation.context(), "cannon", owner, 500.0f, 500.0f);
        return simulation.ecs().getComponent<Damage>(shot).points;
    };
    CHECK(damageOf(0) == 10);
    REQUIRE(simulation.setShip(0, "VX-03"));
    CHECK(damageOf(0) == 13);
    REQUIRE(simulation.setShip(0, "VX-01"));
    CHECK(damageOf(0) == 8);
    CHECK(damageOf(NoOwner) == 10);
}

TEST_CASE("The speed of the ship moves it, and its client predicts it to the bit", "[game]")
{
    const GameConfig config = statsConfig();
    const auto travel = [&config](const std::string &ship) {
        Simulation simulation({}, config);
        simulation.addPlayer(0);
        REQUIRE(simulation.setShip(0, ship));
        const rtype::ecs::Position start = positionOf(simulation, 0);
        simulation.setInput(0, holding(PlayerInput::Up));
        simulation.step();
        return start.y - positionOf(simulation, 0).y;
    };
    const float rules = Rules{}.shipSpeed * TickDuration;
    CHECK(travel("") == Catch::Approx(rules));
    CHECK(travel("VX-01") == Catch::Approx(rules * 1.25f));
    CHECK(travel("VX-03") == Catch::Approx(rules * 0.75f));

    Simulation simulation({}, config);
    simulation.addPlayer(0);
    REQUIRE(simulation.setShip(0, "VX-01"));
    rtype::client::Prediction prediction;
    prediction.setSpeedFactor(statFactor(simulation.statsOf(0).speed));
    prediction.reconcile(positionOf(simulation, 0), simulation.tick());
    for (std::uint32_t tick = 1; tick <= 40; ++tick) {
        const PlayerInput input = holding(tick < 20 ? PlayerInput::Right : PlayerInput::Down);
        prediction.apply(tick, input);
        simulation.setInput(0, input);
        simulation.step();
    }
    REQUIRE(prediction.position());
    CHECK(prediction.position()->x == positionOf(simulation, 0).x);
    CHECK(prediction.position()->y == positionOf(simulation, 0).y);
}

TEST_CASE("LOADOUT carries the ship, with two weapons or none", "[game]")
{
    const LoadoutMessage both{{"player_cannon", "rapid_rifle"}, "VX-03"};
    CHECK(decodeLoadout(encode(both)) == both);
    const LoadoutMessage ship{{}, "VX-01"};
    CHECK(decodeLoadout(encode(ship)) == ship);
    const LoadoutMessage none{};
    CHECK(decodeLoadout(encode(none)) == none);

    CHECK_THROWS_AS(encode(LoadoutMessage{{}, std::string(MaxShipCodeLength + 1, 'V')}),
                    std::invalid_argument);
    // One weapon alone makes it malformed
    std::vector<std::byte> bytes{std::byte{0}, std::byte{1}, std::byte{'a'}, std::byte{0}};
    CHECK_FALSE(decodeLoadout(bytes));
}

TEST_CASE("The gauges of the hangar show the characteristics of hangar.lua", "[client]")
{
    rtype::client::ShipSelection selection;
    rtype::client::readRequiredLevels(selection, {{"VX-01", 1, {0.9f, 0.3f, 0.25f}}});
    CHECK(selection.stats[0] == ShipStats{0.9f, 0.3f, 0.25f});
    // Left out of the file, the ship of the rules
    CHECK(selection.stats[1] == ShipStats{});
    selection.slide = 0.0f;
    CHECK(selection.getBlend(&ShipStats::speed) == 0.9f);
    CHECK(selection.getEquippedCode() == "VX-01");
}

TEST_CASE("Offline, every ship of the hangar is open; online, those above the level lock again", "[client]")
{
    rtype::client::ShipSelection selection;
    rtype::client::readRequiredLevels(selection, {{"VX-01", 1, {}}, {"VX-02", 4, {}}});
    REQUIRE(selection.isLocked(1));

    selection.setAllUnlocked(true);
    CHECK_FALSE(selection.isLocked(1));
    selection.focused = 1;
    selection.equipFocused();
    CHECK(selection.getEquippedCode() == "VX-02");

    // Back online at level 1: the ship equipped offline gives way to the first one open
    selection.setAllUnlocked(false);
    CHECK(selection.isLocked(1));
    CHECK(selection.getEquippedCode() == "VX-01");
}
