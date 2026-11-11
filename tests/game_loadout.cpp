// The two weapons of a player: chosen in the hangar, given to its ship, switched in the game, predicted by
// its client, and sent over the network.

#include "Game/Prediction.hpp"
#include "ShipSelector/ShipSelection.hpp"
#include "rtype/game/Game.hpp"
#include "rtype/game/Messages.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace rtype::game;

namespace
{

/// A cannon that never runs out, and a launcher of 2 missiles that reloads for 30 ticks, both firing every
/// 10 ticks; "laser" is a weapon of the definitions that is not in the hangar.
GameConfig loadoutConfig()
{
    GameConfig config;
    config.projectiles.push_back(
        {"shot", "", ProjectileCategory::Bullet, {rtype::physics::Aabb{{}, {12.0f, 4.0f, 0.0f}}, {}, false}});
    config.weapons.push_back(
        {"cannon", "shot", ProjectileCategory::Bullet, 1500.0f, 10, 0.0f, InfiniteAmmo, 0, 10});
    config.weapons.push_back({"launcher", "shot", ProjectileCategory::Missile, 900.0f, 10, 0.0f, 2, 30, 20});
    config.weapons.push_back(
        {"laser", "shot", ProjectileCategory::Laser, 2000.0f, 10, 0.0f, InfiniteAmmo, 0, 5});
    config.ships.push_back({"fighter", "models/fighter.glb", {}});
    config.player = {"fighter", "cannon", 100};
    config.weaponUnlocks = {{"cannon", 1}, {"launcher", 2}};
    return config;
}

PlayerInput firing(bool second)
{
    PlayerInput input;
    input.set(PlayerInput::Fire, true);
    input.set(PlayerInput::SecondWeapon, second);
    return input;
}

std::size_t countShots(const Snapshot &snapshot)
{
    std::size_t count = 0;
    for (const EntityState &entity : snapshot.entities) {
        count += entity.kind == EntityKind::PlayerShot ? 1 : 0;
    }
    return count;
}

} // namespace

TEST_CASE("A loadout is two different weapons of the hangar, or that of the players", "[game]")
{
    Simulation simulation({}, loadoutConfig());
    CHECK(simulation.isLoadoutAllowed({}));
    CHECK(simulation.isLoadoutAllowed({"cannon", "launcher"}));
    CHECK(simulation.isLoadoutAllowed({"launcher", "cannon"}));
    CHECK_FALSE(simulation.isLoadoutAllowed({"cannon", "cannon"}));
    CHECK_FALSE(simulation.isLoadoutAllowed({"cannon", "laser"}));
    CHECK_FALSE(simulation.isLoadoutAllowed({"cannon", "nope"}));
    CHECK_FALSE(simulation.isLoadoutAllowed({"cannon", ""}));

    // Not for an empty slot, and a refused loadout changes nothing
    CHECK_FALSE(simulation.setLoadout(0, {"cannon", "launcher"}));
    simulation.addPlayer(0);
    CHECK_FALSE(simulation.setLoadout(0, {"cannon", "laser"}));
    CHECK_FALSE(simulation.holsterOf(0));
    CHECK(simulation.weaponOf(0)->weapon == "cannon");

    GameConfig unknown = loadoutConfig();
    unknown.weaponUnlocks.push_back({"nope", 1});
    CHECK_THROWS_AS(Simulation({}, unknown), std::invalid_argument);
}

TEST_CASE("The ship of a player gets its loadout at once and at each respawn, the first weapon in hand",
          "[game]")
{
    Simulation simulation({}, loadoutConfig());
    simulation.addPlayer(0);
    REQUIRE(simulation.setLoadout(0, {"launcher", "cannon"}));
    REQUIRE(simulation.weaponOf(0));
    CHECK(simulation.weaponOf(0)->weapon == "launcher");
    CHECK(simulation.weaponOf(0)->ammo == 2);
    REQUIRE(simulation.holsterOf(0));
    CHECK(simulation.holsterOf(0)->weapon.weapon == "cannon");
    CHECK(simulation.holsterOf(0)->inHand == 0);

    // A new ship has the same weapons, magazines full
    const rtype::ecs::entity_t ship = *findPlayerShip(simulation.ecs(), 0);
    destroyEntity(simulation.ecs(), simulation.context(), ship, DestroyCause::Killed);
    const rtype::ecs::entity_t again = spawnPlayerShip(simulation.ecs(), simulation.context(), 0);
    CHECK(simulation.ecs().getComponent<Armed>(again).weapon == "launcher");
    CHECK(simulation.ecs().getComponent<Holster>(again).weapon.weapon == "cannon");

    // An empty loadout takes the second weapon away
    REQUIRE(simulation.setLoadout(0, {}));
    CHECK(simulation.weaponOf(0)->weapon == "cannon");
    CHECK_FALSE(simulation.holsterOf(0));
}

TEST_CASE("The input takes a weapon in hand, and the one put away keeps its state", "[game]")
{
    GameConfig config = loadoutConfig();
    Simulation simulation({}, config);
    simulation.addPlayer(0);
    REQUIRE(simulation.setLoadout(0, {"launcher", "cannon"}));

    // The two missiles of the launcher, 10 ticks apart: it reloads
    simulation.setInput(0, firing(false));
    for (int tick = 0; tick < 11; ++tick) {
        simulation.step();
    }
    REQUIRE(simulation.weaponOf(0)->weapon == "launcher");
    CHECK(simulation.weaponOf(0)->ammo == 0);
    const std::uint32_t reload = simulation.weaponOf(0)->reloadLeft;
    CHECK(reload > 0);

    // The cannon in hand fires at once; the launcher waits, still reloading
    simulation.setInput(0, firing(true));
    const std::size_t before = countShots(simulation.snapshot());
    simulation.step();
    CHECK(simulation.weaponOf(0)->weapon == "cannon");
    CHECK(simulation.holsterOf(0)->inHand == 1);
    CHECK(simulation.holsterOf(0)->weapon.weapon == "launcher");
    CHECK(simulation.holsterOf(0)->weapon.reloadLeft == reload);
    simulation.step();
    CHECK(countShots(simulation.snapshot()) == before + 1);
    for (int tick = 0; tick < 40; ++tick) {
        simulation.step();
    }
    CHECK(simulation.holsterOf(0)->weapon.reloadLeft == reload);

    // Back to the launcher: it goes on reloading from where it was
    simulation.setInput(0, firing(false));
    simulation.step();
    CHECK(simulation.weaponOf(0)->weapon == "launcher");
    CHECK(simulation.weaponOf(0)->reloadLeft == reload - 1);
    CHECK(simulation.holsterOf(0)->inHand == 0);
}

TEST_CASE("A ship with one weapon ignores the bit of the second", "[game]")
{
    Simulation simulation({}, loadoutConfig());
    simulation.addPlayer(0);
    simulation.setInput(0, firing(true));
    simulation.step();
    CHECK(simulation.weaponOf(0)->weapon == "cannon");
    CHECK_FALSE(simulation.holsterOf(0));
}

TEST_CASE("The prediction switches the weapons as the game does", "[game][client]")
{
    const GameConfig config = loadoutConfig();
    Simulation simulation({}, config);
    simulation.addPlayer(0);
    REQUIRE(simulation.setLoadout(0, {"launcher", "cannon"}));

    const auto weaponOf = [&config](const std::string &id) {
        for (const WeaponDefinition &weapon : config.weapons) {
            if (weapon.id == id) {
                return rtype::client::Prediction::Weapon{weapon, 12.0f};
            }
        }
        return rtype::client::Prediction::Weapon{};
    };
    rtype::client::Prediction prediction;
    prediction.arm({weaponOf("launcher"), weaponOf("cannon")}, 0);
    const EntityState start = [&] {
        for (const EntityState &entity : simulation.snapshot().entities) {
            if (entity.kind == EntityKind::PlayerShip) {
                return entity;
            }
        }
        return EntityState{};
    }();
    prediction.reconcile(rtype::ecs::Position{start.x, start.y}, simulation.tick());

    // Fire, switch, fire, switch back: every tick the game fires at, the prediction fires at too
    std::map<std::uint32_t, ProjectileCategory> fired;
    for (std::uint32_t tick = 1; tick <= 120; ++tick) {
        const PlayerInput input = firing((tick / 25) % 2 == 1);
        prediction.apply(tick, input);
        simulation.setInput(0, input);
        simulation.step();
        // Both weapons fire every 10 ticks: a weapon that just fired waits all of them
        if (simulation.weaponOf(0)->cooldown == 10) {
            fired[tick] = simulation.weaponOf(0)->weapon == "cannon" ? ProjectileCategory::Bullet
                                                                     : ProjectileCategory::Missile;
        }
    }
    REQUIRE(prediction.shots().size() == fired.size());
    for (const auto &[tick, shot] : prediction.shots()) {
        CAPTURE(tick);
        REQUIRE(fired.contains(tick));
        CHECK(shot.mechanic == fired.at(tick));
    }
    CHECK(prediction.weapon()->weapon == simulation.weaponOf(0)->weapon);
    CHECK(prediction.weapon()->ammo == simulation.weaponOf(0)->ammo);
    CHECK(prediction.holster()->weapon.reloadLeft == simulation.holsterOf(0)->weapon.reloadLeft);
}

TEST_CASE("LOADOUT carries the two weapons, and SNAPSHOT the weapon put away", "[game]")
{
    const LoadoutMessage loadout{{"player_cannon", "rapid_rifle"}};
    CHECK(decodeLoadout(encode(loadout)) == loadout);
    CHECK_THROWS_AS(encode(LoadoutMessage{{"player_cannon", ""}}), std::invalid_argument);
    CHECK_FALSE(decodeLoadout({}));
    std::vector<std::byte> extra = encode(loadout);
    extra.push_back(std::byte{0});
    CHECK_FALSE(decodeLoadout(extra));

    SnapshotPart part;
    part.tick = 9;
    part.weapon = {4, 7, 0, 1, 3, 0, 25};
    const std::optional<SnapshotPart> decoded = decodeSnapshotPart(encode(part));
    REQUIRE(decoded);
    CHECK(decoded->weapon == part.weapon);

    // A weapon in hand above 1 makes it malformed
    std::vector<std::byte> bytes = encode(part);
    bytes[17] = std::byte{2};
    CHECK_FALSE(decodeSnapshotPart(bytes));

    // The bit of the second weapon is kept in the inputs
    const InputMessage input{5, {PlayerInput{PlayerInput::SecondWeapon}}};
    CHECK(decodeInput(encode(input))->inputs[0].weaponInHand() == 1);
}

TEST_CASE("The hangar equips two different open weapons and keeps them in the settings", "[client]")
{
    rtype::client::ShipSelection selection;
    rtype::client::readHangarWeapons(selection, {{"cannon", 1}, {"launcher", 2}, {"laser", 1}, {"beam", 3}});
    CHECK(selection.getLoadout() == Loadout{"cannon", "laser"});

    // Past the locked weapons and the one at the other place
    selection.stepWeapon(0, 1);
    CHECK(selection.getLoadout() == Loadout{"cannon", "laser"});
    selection.setLevel(3);
    selection.stepWeapon(0, 1);
    CHECK(selection.getLoadout() == Loadout{"launcher", "laser"});
    selection.stepWeapon(1, -1);
    CHECK(selection.getLoadout() == Loadout{"launcher", "cannon"});
    selection.stepWeapon(1, 1);
    selection.stepWeapon(1, 1);
    CHECK(selection.getLoadout() == Loadout{"launcher", "beam"});

    // A lower level gives the locked weapons way to the first open ones
    selection.setLevel(1);
    CHECK(selection.getLoadout() == Loadout{"cannon", "laser"});

    // Fewer than two weapons open: none to choose
    rtype::client::ShipSelection alone;
    rtype::client::readHangarWeapons(alone, {{"cannon", 1}, {"launcher", 2}});
    CHECK(alone.getLoadout() == Loadout{});

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "rtype_loadout_settings.ini";
    std::filesystem::remove(path);
    {
        rtype::engine::Settings settings(path);
        selection.setLevel(3);
        selection.loadout = {3, 1};
        rtype::client::writeEquippedWeapons(selection, settings);
        CHECK(settings.getString(rtype::client::EquippedWeaponsSettingsKey) == "beam,launcher");

        rtype::client::ShipSelection read;
        rtype::client::readHangarWeapons(read, selection.weapons);
        read.setLevel(3);
        rtype::client::readEquippedWeapons(read, settings);
        CHECK(read.getLoadout() == Loadout{"beam", "launcher"});

        // Locked at the level read: kept as it was
        rtype::client::ShipSelection low;
        rtype::client::readHangarWeapons(low, selection.weapons);
        rtype::client::readEquippedWeapons(low, settings);
        CHECK(low.getLoadout() == Loadout{"cannon", "laser"});
    }
    std::filesystem::remove(path);
}

TEST_CASE("The shots of a player give the projectile of their weapon, for the client to draw its model",
          "[game]")
{
    GameConfig config = loadoutConfig();
    config.projectiles.push_back({"another",
                                  "",
                                  ProjectileCategory::Bullet,
                                  {rtype::physics::Aabb{{}, {6.0f, 2.0f, 0.0f}}, {}, false}});
    config.weapons[1].projectile = "another";
    Simulation simulation({}, config);
    simulation.addPlayer(0);
    REQUIRE(simulation.setLoadout(0, {"cannon", "launcher"}));

    // "another" comes before "shot" among the ids
    CHECK(simulation.context().projectiles.getIndex("another") == 0);
    CHECK(simulation.context().projectiles.getIndex("shot") == 1);
    CHECK(simulation.context().projectiles.getDefinitionAt(1).id == "shot");

    const auto lastShotProjectile = [&simulation] {
        std::optional<std::uint8_t> projectile;
        std::uint32_t newest = 0;
        for (const EntityState &entity : simulation.snapshot().entities) {
            if (entity.kind == EntityKind::PlayerShot && entity.id >= newest) {
                newest = entity.id;
                projectile = entity.ship;
            }
        }
        return projectile;
    };
    simulation.setInput(0, firing(false));
    simulation.step();
    CHECK(lastShotProjectile() == std::uint8_t{1});
    simulation.setInput(0, firing(true));
    simulation.step();
    CHECK(lastShotProjectile() == std::uint8_t{0});
}
