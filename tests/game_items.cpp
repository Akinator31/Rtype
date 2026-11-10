// Tests of the items of the game: their definitions read from config/items/, the waves that make them appear,
// and their effects on the ship that picks them up (ItemSystem, ItemEffects.hpp).

#include "TempDirectory.hpp"
#include "rtype/game/Game.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace rtype::game;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

namespace
{

// --- Configuration ---

struct ConfigDirectory : testing::TempDirectory {
    explicit ConfigDirectory(std::string_view name)
        : testing::TempDirectory(std::string("rtype_tests_items_").append(name))
    {
    }

    void write(const std::string &file, const std::string &code) const
    {
        const std::filesystem::path target = path / file;
        std::filesystem::create_directories(target.parent_path());
        std::ofstream(target, std::ios::binary) << code;
    }

    /// A small valid configuration with one item, which its wave makes appear.
    void writeValid() const
    {
        write("player.lua", "return { ship = 'scout', weapon = 'blaster' }");
        write("projectiles/pulse.lua",
              "return { id = 'pulse', collider = { shape = 'box', halfWidth = 12, halfHeight = 4 } }");
        write("weapons/blaster.lua", "return { id = 'blaster', projectile = 'pulse' }");
        write("ships/scout.lua", "return { id = 'scout', model = 'models/scout.glb' }");
        write("enemies/scout.lua",
              "return { id = 'scout', ship = 'scout', health = 30, weapon = 'blaster' }");
        write("items/kit.lua", "return { id = 'kit', model = 'models/items/kit.glb',"
                               " collider = { shape = 'sphere', radius = 20 },"
                               " effects = { { type = 'heal', amount = 25 },"
                               " { type = 'shield', duration = 2 } } }");
        write("waves/line.lua", "return { name = 'line', difficulty = 1, placement = 'manual', spawns = {"
                                " { enemy = 'scout', x = 2000, y = 200 } },"
                                " items = { { item = 'kit', quantity = 1, maxQuantity = 3 } } }");
    }
};

std::string listErrors(const LoadedGameConfig &loaded)
{
    std::string text;
    for (const std::string &error : loaded.errors) {
        text += error + '\n';
    }
    return text;
}

/// The only error of loading the valid configuration where `file` is replaced by `code`.
std::string errorWith(std::string_view name, const std::string &file, const std::string &code)
{
    const ConfigDirectory directory(name);
    directory.writeValid();
    directory.write(file, code);
    const LoadedGameConfig loaded = loadGameConfig(directory.path, directory.path);
    INFO(listErrors(loaded));
    REQUIRE(loaded.errors.size() == 1);
    return loaded.errors.front();
}

// --- Simulation ---

void run(Simulation &simulation, int ticks)
{
    for (int i = 0; i < ticks; ++i) {
        simulation.step();
    }
}

/// A ship with 100 hit points, a cannon firing every 10 ticks, and the items of each effect, at the size of
/// a ball of 20 units.
GameConfig itemConfig()
{
    GameConfig config;
    config.projectiles.push_back(
        {"shot", "", ProjectileCategory::Bullet, {rtype::physics::Aabb{{}, {12.0f, 4.0f, 0.0f}}, {}, false}});
    config.weapons.push_back(
        {"cannon", "shot", ProjectileCategory::Bullet, 1500.0f, 10, 0.0f, InfiniteAmmo, 0, 10});
    config.ships.push_back({"fighter", "models/fighter.glb", {}});
    config.player = {"fighter", "cannon", 100};
    const rtype::physics::Collider ball{rtype::physics::Sphere{{}, 20.0f}, {}, false};
    const auto item = [&](std::string id, ItemEffect effect) {
        return ItemDefinition{std::move(id), "models/item.glb", ball, {effect}, 10.0f, 0.0f};
    };
    config.items.push_back(item("heal", {ItemEffectType::Heal, 30, 0.0f, 1.0f}));
    config.items.push_back(item("life", {ItemEffectType::ExtraLife, 1, 0.0f, 1.0f}));
    config.items.push_back(item("mine", {ItemEffectType::Damage, 40, 0.0f, 1.0f}));
    config.items.push_back(item("rapid", {ItemEffectType::RapidFire, 0, 2.0f, 2.0f}));
    config.items.push_back(item("shield", {ItemEffectType::Shield, 0, 3.0f, 1.0f}));
    return config;
}

rtype::ecs::entity_t shipOf(Simulation &simulation, std::uint8_t slot)
{
    const std::optional<rtype::ecs::entity_t> ship = findPlayerShip(simulation.ecs(), slot);
    REQUIRE(ship);
    return *ship;
}

/// Puts an item on the ship of the player 0, already appeared, and steps once: the ship picks it up.
void pickUp(Simulation &simulation, const std::string &item)
{
    rtype::ecs::Ecs &ecs = simulation.ecs();
    const rtype::ecs::Position at = ecs.getComponent<rtype::ecs::Position>(shipOf(simulation, 0));
    const rtype::ecs::entity_t entity = spawnItem(ecs, simulation.context(), item, at.x, at.y);
    ecs.getComponent<Item>(entity).age = simulation.rules().itemAppearTicks;
    simulation.step();
}

std::vector<EntityState> itemsOf(const Snapshot &snapshot)
{
    std::vector<EntityState> items;
    std::ranges::copy_if(snapshot.entities, std::back_inserter(items),
                         [](const EntityState &entity) { return entity.kind == EntityKind::Item; });
    return items;
}

/// The number of shots of the player 0 fired in `ticks` ticks with Fire held.
std::size_t countShots(Simulation &simulation, int ticks)
{
    std::size_t shots = 0;
    simulation.setInput(0, PlayerInput{PlayerInput::Fire});
    for (int tick = 0; tick < ticks; ++tick) {
        simulation.step();
        const std::vector<EntityState> &entities = simulation.snapshot().entities;
        shots = static_cast<std::size_t>(
            std::ranges::count(entities, EntityKind::PlayerShot, &EntityState::kind));
    }
    return shots;
}

} // namespace

// --- Configuration ---

TEST_CASE("An item file is read with its model, its hitbox and its effects, and a wave names it", "[game]")
{
    const ConfigDirectory directory("valid");
    directory.writeValid();

    const LoadedGameConfig loaded = loadGameConfig(directory.path, directory.path);

    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    REQUIRE(loaded.config.items.size() == 1);
    const ItemDefinition &kit = loaded.config.items.front();
    CHECK(kit.id == "kit");
    CHECK(kit.model == "models/items/kit.glb");
    CHECK(std::get<rtype::physics::Sphere>(kit.collider.shape).radius == 20.0f);
    REQUIRE(kit.effects.size() == 2);
    CHECK(kit.effects[0].type == ItemEffectType::Heal);
    CHECK(kit.effects[0].amount == 25);
    CHECK(kit.effects[1].type == ItemEffectType::Shield);
    CHECK(kit.effects[1].duration == 2.0f);
    CHECK(kit.lifetime == 10.0f);
    CHECK(kit.speed == 60.0f);
    const WaveDefinition &wave = loaded.config.waves.front();
    REQUIRE(wave.items.size() == 1);
    CHECK(wave.items[0].item == "kit");
    CHECK(wave.items[0].quantity == 1);
    CHECK(wave.items[0].maxQuantity == 3);
}

TEST_CASE("An item file is refused without an effect, with an unknown one, or without its numbers", "[game]")
{
    const std::string head =
        "return { id = 'kit', model = 'm.glb', collider = { shape = 'sphere', radius = 5 }, ";
    CHECK(errorWith("no_effect", "items/kit.lua", head + "effects = {} }") ==
          "items/kit.lua: effects: expected at least one effect");
    CHECK_THAT(errorWith("unknown_effect", "items/kit.lua", head + "effects = { { type = 'fly' } } }"),
               StartsWith("items/kit.lua: effects[1].type: expected ") && ContainsSubstring("rapid_fire"));
    CHECK(errorWith("no_amount", "items/kit.lua", head + "effects = { { type = 'heal' } } }") ==
          "items/kit.lua: effects[1].amount: missing");
    {
        // Refused by its registry: the wave that names it fails too
        const ConfigDirectory directory("slow_rapid");
        directory.writeValid();
        directory.write("items/kit.lua",
                        head + "effects = { { type = 'rapid_fire', factor = 0.5, duration = 3 } } }");
        const LoadedGameConfig loaded = loadGameConfig(directory.path, directory.path);
        REQUIRE_FALSE(loaded.errors.empty());
        CHECK(loaded.errors.front() ==
              "items/kit.lua: the rapid fire of the item 'kit' needs a factor above 1");
    }
    CHECK(errorWith("extra_field", "items/kit.lua",
                    head + "effects = { { type = 'shield', duration = 3, amount = 2 } } }") ==
          "items/kit.lua: effects[1].amount: unknown field");
    CHECK(errorWith("no_model", "items/kit.lua",
                    "return { id = 'kit', collider = { shape = 'sphere', radius = 5 },"
                    " effects = { { type = 'shield', duration = 3 } } }") == "items/kit.lua: model: missing");
}

TEST_CASE("A wave naming an unknown item is refused", "[game]")
{
    CHECK(errorWith("unknown_item", "waves/line.lua",
                    "return { name = 'line', difficulty = 1, placement = 'manual', spawns = {"
                    " { enemy = 'scout', x = 2000, y = 200 } }, items = { { item = 'gold' } } }") ==
          "waves/line.lua: items: unknown item 'gold'");
}

TEST_CASE("The items of the repository load, each with a model, and the waves make them appear", "[game]")
{
    const LoadedGameConfig loaded = loadGameConfig(RTYPE_TEST_CONFIG);
    INFO(listErrors(loaded));
    REQUIRE(loaded.isValid());
    Simulation simulation({}, loaded.config);
    for (const char *id : {"repair_kit", "extra_life", "bydo_mine", "rapid_fire", "shield"}) {
        INFO(id);
        REQUIRE(simulation.context().items.hasDefinition(id));
        const std::filesystem::path model =
            std::filesystem::path(RTYPE_TEST_CONFIG) / ".." / "apps" / "client" / "assets" /
            simulation.context().items.getDefinition(id).model;
        CHECK(std::filesystem::is_regular_file(model));
    }
}

TEST_CASE("An item registry refuses an item without a model or an effect, or an effect without its numbers",
          "[game]")
{
    ItemRegistry registry;
    const rtype::physics::Collider ball{rtype::physics::Sphere{{}, 10.0f}, {}, false};
    CHECK_THROWS_AS(registry.addDefinition({"kit", "", ball, {{ItemEffectType::Heal, 5}}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition({"kit", "m.glb", ball, {}}), std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition({"kit", "m.glb", ball, {{ItemEffectType::Heal, 0}}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition({"kit", "m.glb", ball, {{ItemEffectType::Shield, 0, 0.0f}}}),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        registry.addDefinition({"kit", "m.glb", ball, {{ItemEffectType::RapidFire, 0, 2.0f, 1.0f}}}),
        std::invalid_argument);
    registry.addDefinition({"kit", "m.glb", ball, {{ItemEffectType::Heal, 5}}});
    registry.addDefinition({"another", "m.glb", ball, {{ItemEffectType::Heal, 5}}});
    CHECK_THROWS_AS(registry.addDefinition({"kit", "m.glb", ball, {{ItemEffectType::Heal, 5}}}),
                    std::invalid_argument);
    // By alphabetical order
    CHECK(registry.getIndex("another") == 0);
    CHECK(registry.getIndex("kit") == 1);
}

// --- In the game ---

TEST_CASE("An item is sent to the clients as an Item, with the index of its definition", "[game]")
{
    Simulation simulation({}, itemConfig());
    spawnItem(simulation.ecs(), simulation.context(), "mine", 1000.0f, 500.0f);
    simulation.step();

    const std::vector<EntityState> items = itemsOf(simulation.snapshot());
    REQUIRE(items.size() == 1);
    CHECK(items[0].ship == simulation.context().items.getIndex("mine"));
    CHECK(items[0].owner == NoOwner);
}

TEST_CASE("A heal gives back hit points, up to the health of the player", "[game]")
{
    Simulation simulation({}, itemConfig());
    simulation.addPlayer(0);
    rtype::ecs::Ecs &ecs = simulation.ecs();
    ecs.getComponent<Health>(shipOf(simulation, 0)).points = 50;

    pickUp(simulation, "heal");
    CHECK(ecs.getComponent<Health>(shipOf(simulation, 0)).points == 80);
    CHECK(itemsOf(simulation.snapshot()).empty());
    REQUIRE(simulation.snapshot().destroyed.size() == 1);
    CHECK(simulation.snapshot().destroyed[0].cause == DestroyCause::Killed);

    pickUp(simulation, "heal");
    CHECK(ecs.getComponent<Health>(shipOf(simulation, 0)).points == 100);
}

TEST_CASE("An extra life gives the player one life more", "[game]")
{
    Simulation simulation({}, itemConfig());
    simulation.addPlayer(0);
    const std::uint8_t lives = simulation.context().players[0].lives;

    pickUp(simulation, "life");
    CHECK(simulation.context().players[0].lives == lives + 1);
}

TEST_CASE("A damage takes hit points, and loses the ship that has none left, but not a shielded one",
          "[game]")
{
    Simulation simulation({}, itemConfig());
    simulation.addPlayer(0);
    rtype::ecs::Ecs &ecs = simulation.ecs();
    const std::uint8_t lives = simulation.context().players[0].lives;

    pickUp(simulation, "mine");
    CHECK(ecs.getComponent<Health>(shipOf(simulation, 0)).points == 60);

    pickUp(simulation, "shield");
    pickUp(simulation, "mine");
    CHECK(ecs.getComponent<Health>(shipOf(simulation, 0)).points == 60);

    ecs.getComponent<Invulnerable>(shipOf(simulation, 0)).ticks = 0;
    ecs.getComponent<Health>(shipOf(simulation, 0)).points = 30;
    pickUp(simulation, "mine");
    CHECK_FALSE(findPlayerShip(ecs, 0));
    CHECK(simulation.context().players[0].lives == lives - 1);
    CHECK(simulation.context().players[0].respawnIn > 0);
}

TEST_CASE("A shield makes the ship invulnerable for its duration", "[game]")
{
    Simulation simulation({}, itemConfig());
    simulation.addPlayer(0);

    pickUp(simulation, "shield");
    const rtype::ecs::entity_t ship = shipOf(simulation, 0);
    REQUIRE(simulation.ecs().hasComponent<Invulnerable>(ship));
    // Three seconds, less the tick of the hits that went by
    CHECK(simulation.ecs().getComponent<Invulnerable>(ship).ticks >= 3 * TickRate - 1);
    simulation.step();
    const auto states = simulation.snapshot().entities;
    const auto found = std::ranges::find(states, EntityKind::PlayerShip, &EntityState::kind);
    REQUIRE(found != states.end());
    CHECK((found->flags & EntityState::Invulnerable) != 0);
}

TEST_CASE("A rapid fire divides the fire interval for its duration, then the weapon fires as before",
          "[game]")
{
    Simulation simulation({}, itemConfig());
    simulation.addPlayer(0);

    pickUp(simulation, "rapid");
    const std::optional<Armed> armed = simulation.weaponOf(0);
    REQUIRE(armed);
    CHECK(armed->boostInterval == 5);
    CHECK(armed->boostTicks == 2 * TickRate);

    // Twice as many shots while boosted: one every 5 ticks instead of 10
    const std::size_t boosted = countShots(simulation, 30);
    CHECK(boosted == 6);
    run(simulation, 2 * TickRate);
    CHECK(simulation.weaponOf(0)->boostTicks == 0);
}

TEST_CASE("An item vanishes after its lifetime, and is not picked up while it appears", "[game]")
{
    Rules rules;
    rules.itemAppearTicks = 10;
    GameConfig config = itemConfig();
    config.items[0].lifetime = 0.5f;
    Simulation simulation(rules, config);
    simulation.addPlayer(0);
    const rtype::ecs::Position at =
        simulation.ecs().getComponent<rtype::ecs::Position>(shipOf(simulation, 0));
    spawnItem(simulation.ecs(), simulation.context(), "heal", at.x, at.y);
    simulation.ecs().getComponent<Health>(shipOf(simulation, 0)).points = 50;

    run(simulation, 9);
    CHECK(itemsOf(simulation.snapshot()).size() == 1);
    CHECK(simulation.ecs().getComponent<Health>(shipOf(simulation, 0)).points == 50);
    simulation.step();
    CHECK(itemsOf(simulation.snapshot()).empty());
    CHECK(simulation.ecs().getComponent<Health>(shipOf(simulation, 0)).points == 80);

    spawnItem(simulation.ecs(), simulation.context(), "heal", 1500.0f, 200.0f);
    run(simulation, 29);
    CHECK(itemsOf(simulation.snapshot()).size() == 1);
    simulation.step();
    CHECK(itemsOf(simulation.snapshot()).empty());
    REQUIRE(simulation.snapshot().destroyed.size() == 1);
    CHECK(simulation.snapshot().destroyed[0].cause == DestroyCause::Removed);
}

TEST_CASE("A wave makes its items appear at random places of the screen, as many as it draws", "[game]")
{
    GameConfig config = itemConfig();
    EnemyDefinition scout;
    scout.id = "scout";
    scout.ship = "fighter";
    scout.health = 10;
    scout.weapon = "cannon";
    config.enemies.push_back(scout);
    WaveDefinition wave;
    wave.name = "line";
    wave.quantity = 4;
    wave.spawnInterval = 1.0f;
    wave.placement = WavePlacement::Random;
    wave.enemy = "scout";
    wave.area = {1900.0f, 1900.0f, 900.0f, 900.0f};
    wave.items = {{"heal", 2, 0}, {"shield", 1, 3}};
    config.waves.push_back(wave);
    Rules rules;
    rules.firstWaveDelay = 1;
    rules.seed = 42;
    Simulation simulation(rules, config);
    simulation.addPlayer(0);

    std::vector<std::uint32_t> seen;
    std::size_t heals = 0;
    std::size_t shields = 0;
    for (int tick = 0; tick < 4 * TickRate; ++tick) {
        simulation.step();
        for (const EntityState &item : itemsOf(simulation.snapshot())) {
            if (std::ranges::find(seen, item.id) != seen.end()) {
                continue;
            }
            seen.push_back(item.id);
            CHECK(item.x >= rules.itemMinX);
            CHECK(item.x <= GameWidth - rules.itemMargin);
            CHECK(item.y >= rules.itemMargin);
            CHECK(item.y <= GameHeight - rules.itemMargin);
            (item.ship == simulation.context().items.getIndex("heal") ? heals : shields) += 1;
        }
    }
    CHECK(heals == 2);
    CHECK(shields >= 1);
    CHECK(shields <= 3);
}
