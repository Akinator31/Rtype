// Tests of the configuration of the game: Lua files read into definitions (ADR 0018,
// docs/guides/configure-enemies-and-weapons.md).

#include "rtype/game/Game.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

using namespace rtype::game;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;

namespace
{

// A configuration directory of its own for each test, removed at the end
struct ConfigDirectory {
    std::filesystem::path path;

    explicit ConfigDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::string("rtype_tests_config_").append(name))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~ConfigDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    ConfigDirectory(const ConfigDirectory &) = delete;
    ConfigDirectory &operator=(const ConfigDirectory &) = delete;

    void write(const std::string &file, const std::string &code) const
    {
        const std::filesystem::path target = path / file;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream(target) << code;
    }

    /// A small valid configuration: one of each kind, and the player.
    void writeValid() const
    {
        write("player.lua", "return { ship = 'scout', weapon = 'blaster' }");
        write("projectiles/pulse.lua",
              "return { id = 'pulse', collider = { shape = 'box', halfWidth = 12, halfHeight = 4 } }");
        write("weapons/blaster.lua", "return { id = 'blaster', projectile = 'pulse' }");
        write("ships/scout.lua", "return { id = 'scout', model = 'models/scout.glb' }");
        write("enemies/scout.lua",
              "return { id = 'scout', ship = 'scout', health = 30, weapon = 'blaster' }");
        write("waves/line.lua",
              "return { name = 'line', difficulty = 1, placement = 'manual', spawns = {"
              " { enemy = 'scout', x = 2000, y = 200 }, { enemy = 'scout', x = 2000, y = 800 } } }");
    }
};

bool hasError(const LoadedGameConfig &loaded, const std::string &error)
{
    return std::ranges::find(loaded.errors, error) != loaded.errors.end();
}

std::string listErrors(const LoadedGameConfig &loaded)
{
    std::string text;
    for (const std::string &error : loaded.errors) {
        text += error + '\n';
    }
    return text;
}

/// The only error of loading a valid configuration where `file` is replaced by `code`.
std::string errorWith(std::string_view name, const std::string &file, const std::string &code)
{
    const ConfigDirectory directory(name);
    directory.writeValid();
    directory.write(file, code);
    const LoadedGameConfig loaded = loadGameConfig(directory.path);
    INFO(listErrors(loaded));
    REQUIRE(loaded.errors.size() == 1);
    return loaded.errors.front();
}

} // namespace

TEST_CASE("The configuration of the repository loads, and a Simulation registers it", "[game]")
{
    const LoadedGameConfig loaded = loadGameConfig(RTYPE_TEST_CONFIG);

    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    Simulation simulation({}, loaded.config);
    const GameContext &context = simulation.context();
    CHECK(context.projectiles.hasDefinition("pulse"));
    CHECK(context.weapons.hasDefinition("blaster"));
    CHECK(context.ships.hasDefinition("bydo_scout"));
    CHECK(context.enemies.hasDefinition("bydo_scout"));
    CHECK(context.waves.hasDefinition("scouts_line"));
    CHECK(context.weapons.hasDefinition(context.player.weapon));
    CHECK(context.ships.hasDefinition(context.player.ship));
    // A wave of each difficulty: the game draws one of each
    for (std::uint8_t difficulty = MinWaveDifficulty; difficulty <= MaxWaveDifficulty; ++difficulty) {
        INFO("difficulty " << int{difficulty});
        CHECK_FALSE(context.waves.listWavesOfDifficulty(difficulty).empty());
    }
}

TEST_CASE("Each kind of file is read into its definition, with the defaults of the optional fields", "[game]")
{
    const ConfigDirectory directory("kinds");
    directory.writeValid();
    directory.write("projectiles/orb.lua",
                    "return { id = 'orb', model = 'models/orb.glb', category = 'plasma',"
                    " collider = { shape = 'sphere', radius = 6, offsetX = 2, trigger = true } }");
    directory.write("weapons/gun.lua",
                    "return { id = 'gun', projectile = 'orb', mechanic = 'missile',"
                    " projectileSpeed = 800, fireInterval = 4.0, range = 500, magazineSize = 8,"
                    " reloadTicks = 90, damage = 40 }");
    directory.write("ships/tank.lua",
                    "return { id = 'tank', model = 'models/tank.glb',"
                    " sounds = { move = 'sounds/move.ogg', destruction = 'sounds/boom.ogg' },"
                    " collider = { shape = 'box', halfWidth = 60, halfHeight = 30 } }");
    directory.write("enemies/tank.lua", "return { id = 'tank', ship = 'tank', health = 255, speed = 100,"
                                        " acceleration = 0, weapon = 'gun', score = 500,"
                                        " collisionDamage = 0 }");
    directory.write("player.lua", "return { ship = 'tank', weapon = 'gun', health = 50 }");
    directory.write("waves/tanks.lua",
                    "return { name = 'tanks', label = 'Tanks', description = 'Three tanks', difficulty = 5,"
                    " placement = 'random', enemy = 'tank', quantity = 3, spawnInterval = 0.5,"
                    " area = { minX = 2000, maxY = 500 } }");

    const LoadedGameConfig loaded = loadGameConfig(directory.path);

    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    const GameConfig &config = loaded.config;
    REQUIRE(config.projectiles.size() == 2);
    const ProjectileDefinition &orb = config.projectiles[0];
    CHECK(orb.id == "orb");
    CHECK(orb.model == "models/orb.glb");
    CHECK(orb.category == ProjectileCategory::Plasma);
    REQUIRE(std::holds_alternative<rtype::physics::Sphere>(orb.collider.shape));
    CHECK(std::get<rtype::physics::Sphere>(orb.collider.shape).radius == Approx(6.0f));
    CHECK(orb.collider.offset.x == Approx(2.0f));
    CHECK(orb.collider.trigger);
    const ProjectileDefinition &pulse = config.projectiles[1];
    CHECK(pulse.model.empty());
    CHECK(pulse.category == ProjectileCategory::Bullet);
    REQUIRE(std::holds_alternative<rtype::physics::Aabb>(pulse.collider.shape));
    CHECK(std::get<rtype::physics::Aabb>(pulse.collider.shape).halfSize.x == Approx(12.0f));
    CHECK(std::get<rtype::physics::Aabb>(pulse.collider.shape).halfSize.y == Approx(4.0f));

    REQUIRE(config.weapons.size() == 2);
    const WeaponDefinition &blaster = config.weapons[0];
    const WeaponDefinition defaults;
    CHECK(blaster.mechanic == defaults.mechanic);
    CHECK(blaster.fireInterval == defaults.fireInterval);
    CHECK(blaster.magazineSize == InfiniteAmmo);
    CHECK(blaster.damage == defaults.damage);
    const WeaponDefinition &gun = config.weapons[1];
    CHECK(gun.mechanic == ProjectileCategory::Missile);
    CHECK(gun.projectileSpeed == Approx(800.0f));
    CHECK(gun.fireInterval == 4);
    CHECK(gun.range == Approx(500.0f));
    CHECK(gun.magazineSize == 8);
    CHECK(gun.reloadTicks == 90);
    CHECK(gun.damage == 40);

    REQUIRE(config.ships.size() == 2);
    const ShipDefinition &scoutShip = config.ships[0];
    CHECK(halfSizeOf(scoutShip.collider).x == Approx(halfSizeOf(ShipDefinition{}.collider).x));
    const ShipDefinition &tank = config.ships[1];
    CHECK(halfSizeOf(tank.collider).x == Approx(60.0f));
    CHECK(halfSizeOf(tank.collider).y == Approx(30.0f));
    CHECK(tank.sounds.move == "sounds/move.ogg");
    CHECK(tank.sounds.destruction == "sounds/boom.ogg");
    CHECK(tank.sounds.collision.empty());

    REQUIRE(config.enemies.size() == 2);
    const EnemyDefinition &scout = config.enemies[0];
    CHECK(scout.speed == Approx(EnemyDefinition{}.speed));
    CHECK(scout.score == EnemyDefinition{}.score);
    CHECK(scout.collisionDamage == EnemyDefinition{}.collisionDamage);
    CHECK_FALSE(scout.behavior);
    const EnemyDefinition &enemy = config.enemies[1];
    CHECK(enemy.health == 255);
    CHECK(enemy.acceleration == Approx(0.0f));
    CHECK(enemy.score == 500);
    CHECK(enemy.collisionDamage == 0);

    CHECK(config.player.ship == "tank");
    CHECK(config.player.weapon == "gun");
    CHECK(config.player.health == 50);

    REQUIRE(config.waves.size() == 2);
    const WaveDefinition &line = config.waves[0];
    CHECK(line.difficulty == 1);
    CHECK(line.placement == WavePlacement::Manual);
    CHECK(line.quantity == 2);
    REQUIRE(line.spawns.size() == 2);
    CHECK(line.spawns[1].y == Approx(800.0f));
    const WaveDefinition &tanks = config.waves[1];
    CHECK(tanks.label == "Tanks");
    CHECK(tanks.difficulty == 5);
    CHECK(tanks.quantity == 3);
    CHECK(tanks.spawnInterval == Approx(0.5f));
    CHECK(tanks.area.minX == Approx(2000.0f));
    CHECK(tanks.area.maxX == Approx(WaveArea{}.maxX));
    CHECK(tanks.area.maxY == Approx(500.0f));
}

TEST_CASE("The files are read from sub-folders too, in the order of their paths, and the others ignored",
          "[game]")
{
    const ConfigDirectory directory("order");
    directory.writeValid();
    directory.write("projectiles/b.lua", "return { id = 'b', collider = { shape = 'sphere', radius = 1 } }");
    directory.write("projectiles/a/c.lua",
                    "return { id = 'c', collider = { shape = 'sphere', radius = 1 } }");
    directory.write("projectiles/a.lua", "return { id = 'a', collider = { shape = 'sphere', radius = 1 } }");
    directory.write("projectiles/notes.txt", "not Lua");

    const LoadedGameConfig loaded = loadGameConfig(directory.path);

    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    REQUIRE(loaded.config.projectiles.size() == 4);
    CHECK(loaded.config.projectiles[0].id == "c");
    CHECK(loaded.config.projectiles[1].id == "a");
    CHECK(loaded.config.projectiles[2].id == "b");
    CHECK(loaded.config.projectiles[3].id == "pulse");
}

TEST_CASE("A directory without player.lua is an error, as a missing one", "[game]")
{
    const ConfigDirectory directory("empty");

    const LoadedGameConfig empty = loadGameConfig(directory.path);
    REQUIRE(empty.errors.size() == 1);
    CHECK(empty.errors.front() == "player.lua: missing: the players need a ship and a weapon");
    CHECK(empty.config.projectiles.empty());

    const LoadedGameConfig missing = loadGameConfig(directory.path / "nope");
    REQUIRE(missing.errors.size() == 1);
    CHECK_THAT(missing.errors.front(), ContainsSubstring("no configuration directory"));
}

TEST_CASE("A wrong field is an error naming the file and the field", "[game]")
{
    CHECK(errorWith("health_zero", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 0, weapon = 'blaster' }") ==
          "enemies/scout.lua: health: expected an integer from 1 to 255");
    CHECK(errorWith("health_fraction", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 2.5, weapon = 'blaster' }") ==
          "enemies/scout.lua: health: expected an integer from 1 to 255");
    CHECK(errorWith("health_string", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = '30', weapon = 'blaster' }") ==
          "enemies/scout.lua: health: expected an integer from 1 to 255");
    CHECK(errorWith("speed", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 1, speed = -1, weapon = 'blaster' }") ==
          "enemies/scout.lua: speed: expected a number above 0");
    CHECK(errorWith("no_id", "enemies/scout.lua",
                    "return { ship = 'scout', health = 1, weapon = 'blaster' }") ==
          "enemies/scout.lua: id: missing");
    CHECK(errorWith("empty_id", "ships/scout.lua", "return { id = '', model = 'm.glb' }") ==
          "ships/scout.lua: id: expected a non-empty string");
    CHECK(errorWith("misspelt", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 1, weapon = 'blaster', sped = 3 }") ==
          "enemies/scout.lua: sped: unknown field");
    CHECK(errorWith(
              "category", "projectiles/pulse.lua",
              "return { id = 'pulse', category = 'lazer', collider = { shape = 'sphere', radius = 1 } }") ==
          R"(projectiles/pulse.lua: category: expected "bullet", "missile", "laser" or "plasma")");
    CHECK(errorWith("no_collider", "projectiles/pulse.lua", "return { id = 'pulse' }") ==
          "projectiles/pulse.lua: collider: missing");
    CHECK(errorWith("collider", "projectiles/pulse.lua",
                    "return { id = 'pulse', collider = { shape = 'sphere', halfWidth = 1 } }") ==
          "projectiles/pulse.lua: collider.radius: missing");
    CHECK(errorWith("fire_interval", "weapons/blaster.lua",
                    "return { id = 'blaster', projectile = 'pulse', fireInterval = 0 }") ==
          "weapons/blaster.lua: fireInterval: expected an integer from 1 to 4294967295");
    CHECK(errorWith("sounds", "ships/scout.lua",
                    "return { id = 'scout', model = 'm.glb', sounds = { move = 3 } }") ==
          "ships/scout.lua: sounds.move: expected a string");
    CHECK(errorWith(
              "spawn", "waves/line.lua",
              "return { name = 'line', difficulty = 1, placement = 'manual',"
              " spawns = { { enemy = 'scout', x = 0, y = 0 },"
              " { x = 0, y = 0 } } }") == "waves/line.lua: spawns[2].enemy: missing");
    CHECK(errorWith("spawns", "waves/line.lua",
                    "return { name = 'line', difficulty = 1, placement = 'manual', spawns = 3 }") ==
          "waves/line.lua: spawns: expected a list of tables");
    CHECK(errorWith("random_quantity", "waves/line.lua",
                    "return { name = 'line', difficulty = 1, placement = 'random', enemy = 'scout' }") ==
          "waves/line.lua: quantity: missing");
    CHECK(errorWith("no_difficulty", "waves/line.lua",
                    "return { name = 'line', placement = 'random', enemy = 'scout', quantity = 1 }") ==
          "waves/line.lua: difficulty: missing");
    CHECK(errorWith("damage", "weapons/blaster.lua",
                    "return { id = 'blaster', projectile = 'pulse', damage = 0 }") ==
          "weapons/blaster.lua: damage: expected an integer from 1 to 255");
    CHECK(errorWith("player_health", "player.lua",
                    "return { ship = 'scout', weapon = 'blaster', health = 0 }") ==
          "player.lua: health: expected an integer from 1 to 255");
    CHECK(errorWith("player_weapon", "player.lua", "return { ship = 'scout' }") ==
          "player.lua: weapon: missing");
    CHECK(errorWith("update", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 1, weapon = 'blaster', update = 3 }") ==
          "enemies/scout.lua: update: expected a function");
    CHECK(errorWith("difficulty", "waves/line.lua",
                    "return { name = 'line', difficulty = 6, placement = 'random', enemy = 'scout',"
                    " quantity = 1 }") == "waves/line.lua: difficulty: expected an integer from 1 to 5");
}

TEST_CASE("A file that does not return a table, or does not run, is an error naming it", "[game]")
{
    CHECK(errorWith("nothing", "ships/scout.lua", "local x = 1") ==
          "ships/scout.lua: expected the file to return a table, `return { ... }`");
    CHECK_THAT(errorWith("syntax", "ships/scout.lua", "return { id = 'scout',"),
               ContainsSubstring("ships/scout.lua:1:"));
    CHECK_THAT(errorWith("runtime", "ships/scout.lua", "return { id = nothing.field }"),
               ContainsSubstring("ships/scout.lua:1:"));
    CHECK_THAT(errorWith("loop", "ships/scout.lua", "while true do end"),
               ContainsSubstring("instruction limit"));
    CHECK_THAT(errorWith("os", "ships/scout.lua", "return { id = 'scout', model = os.time() }"),
               ContainsSubstring("ships/scout.lua:1:"));
}

TEST_CASE("An enemy with an update function keeps the code of its file, its behavior", "[game]")
{
    const ConfigDirectory directory("behavior");
    directory.writeValid();
    const std::string code = "return { id = 'scout', ship = 'scout', health = 30, weapon = 'blaster',\n"
                             "    update = function(enemy, game) enemy:fire() end }\n";
    directory.write("enemies/scout.lua", code);

    const LoadedGameConfig loaded = loadGameConfig(directory.path);
    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    const std::optional<EnemyBehavior> &behavior = loaded.config.enemies.at(0).behavior;
    REQUIRE(behavior);
    CHECK(behavior->file == "enemies/scout.lua");
    CHECK(behavior->code == code);
}

TEST_CASE("A file can compute its values with the sandbox", "[game]")
{
    const ConfigDirectory directory("compute");
    directory.writeValid();
    directory.write("projectiles/computed.lua", R"(
        local size = 4
        return { id = string.format('pulse_%d', size), collider = { shape = 'box', halfWidth = size * 3,
                 halfHeight = math.max(size, 2) } }
    )");

    const LoadedGameConfig loaded = loadGameConfig(directory.path);

    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    CHECK(loaded.config.projectiles.front().id == "pulse_4");
}

TEST_CASE("An unknown id named by a definition is an error of its file", "[game]")
{
    CHECK(errorWith("projectile", "weapons/blaster.lua", "return { id = 'blaster', projectile = 'nope' }") ==
          "weapons/blaster.lua: projectile: unknown projectile 'nope'");
    CHECK(errorWith("ship", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'nope', health = 1, weapon = 'blaster' }") ==
          "enemies/scout.lua: ship: unknown ship 'nope'");
    CHECK(errorWith("weapon", "enemies/scout.lua",
                    "return { id = 'scout', ship = 'scout', health = 1, weapon = 'nope' }") ==
          "enemies/scout.lua: weapon: unknown weapon 'nope'");
    CHECK(errorWith("wave", "waves/line.lua",
                    "return { name = 'line', difficulty = 1, placement = 'random', enemy = 'nope',"
                    " quantity = 2 }") ==
          "waves/line.lua: the wave 'line' uses the unknown enemy 'nope'");
    CHECK(errorWith("player_ship", "player.lua", "return { ship = 'nope', weapon = 'blaster' }") ==
          "player.lua: ship: unknown ship 'nope'");
    CHECK(errorWith("player_unknown_weapon", "player.lua", "return { ship = 'scout', weapon = 'nope' }") ==
          "player.lua: weapon: unknown weapon 'nope'");
}

TEST_CASE("An id defined twice is an error of the second file", "[game]")
{
    CHECK(errorWith("double", "ships/scout_copy.lua", "return { id = 'scout', model = 'other.glb' }") ==
          "ships/scout_copy.lua: the ship 'scout' is already defined");
}

TEST_CASE("Every error is reported, and a configuration with errors is empty", "[game]")
{
    const ConfigDirectory directory("errors");
    directory.writeValid();
    directory.write("ships/broken.lua", "return { id = 'broken' }");
    directory.write("enemies/broken.lua", "return 3");

    const LoadedGameConfig loaded = loadGameConfig(directory.path);

    INFO(listErrors(loaded));
    CHECK_FALSE(loaded.isValid());
    CHECK(loaded.errors.size() == 2);
    CHECK(hasError(loaded, "ships/broken.lua: model: missing"));
    CHECK(hasError(loaded, "enemies/broken.lua: expected the file to return a table, `return { ... }`"));
    CHECK(loaded.config.projectiles.empty());
    CHECK(loaded.config.enemies.empty());
}
