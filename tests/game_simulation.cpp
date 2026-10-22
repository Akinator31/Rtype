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

namespace
{

/// An entity put in the world by hand, without velocity: it stays where it is.
void place(Simulation &simulation, EntityKind kind, std::uint8_t owner, float x, float y,
           std::uint32_t networkId)
{
    rtype::ecs::Ecs &ecs = simulation.ecs();
    const rtype::ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<NetworkId>(entity, networkId);
    ecs.addComponent<Kind>(entity, kind);
    ecs.addComponent<Owner>(entity, owner);
    ecs.addComponent<rtype::ecs::Position>(entity, x, y);
    ecs.addComponent<Health>(entity, std::uint8_t{1});
    ecs.addComponent<rtype::physics::Collider>(entity, rtype::physics::Aabb{{}, {10.0f, 10.0f, 0.0f}},
                                               rtype::physics::Vec3{}, false);
}

bool wasDestroyed(const Snapshot &snapshot, std::uint32_t id, DestroyCause cause)
{
    return std::ranges::find(snapshot.destroyed, DestroyedEntity{id, cause}) != snapshot.destroyed.end();
}

} // namespace

TEST_CASE("A shot of a Bydo destroys a ship, which comes back invulnerable after a delay", "[game]")
{
    const Rules rules;
    Simulation simulation(rules);
    simulation.addPlayer(0);
    simulation.step();
    const EntityState ship = *shipOf(simulation.snapshot(), 0);
    place(simulation, EntityKind::BydoShot, NoOwner, ship.x, ship.y, 1000);

    simulation.step();

    CHECK(wasDestroyed(simulation.snapshot(), ship.id, DestroyCause::Killed));
    CHECK(!shipOf(simulation.snapshot(), 0));
    CHECK(simulation.snapshot().players[0].lives == rules.lives - 1);
    CHECK(!simulation.snapshot().gameOver);

    run(simulation, static_cast<int>(rules.respawnDelay));
    const std::optional<EntityState> back = shipOf(simulation.snapshot(), 0);
    REQUIRE(back);
    CHECK((back->flags & EntityState::Invulnerable) != 0);

    // Invulnerable: a shot on it does nothing. The shot has no velocity: it stays on the ship
    place(simulation, EntityKind::BydoShot, NoOwner, back->x, back->y, 1001);
    simulation.step();
    REQUIRE(shipOf(simulation.snapshot(), 0));

    // The ship stays, invulnerable, then the flag ends within invulnerableTicks ticks of its return
    const auto invulnerable = [&] {
        const std::optional<EntityState> current = shipOf(simulation.snapshot(), 0);
        return current && (current->flags & EntityState::Invulnerable) != 0;
    };
    std::uint32_t ticks = 1;
    while (invulnerable() && ticks <= rules.invulnerableTicks) {
        simulation.step();
        ++ticks;
    }
    CHECK(ticks <= rules.invulnerableTicks);
    const std::optional<EntityState> vulnerable = shipOf(simulation.snapshot(), 0);
    REQUIRE(vulnerable);
    CHECK((vulnerable->flags & EntityState::Invulnerable) == 0);

    // Then the shot still on it hits it
    simulation.step();
    CHECK(wasDestroyed(simulation.snapshot(), vulnerable->id, DestroyCause::Killed));
    CHECK(simulation.snapshot().players[0].lives == rules.lives - 2);
}

TEST_CASE("The game is lost once every player is out of lives, and it is told once", "[game]")
{
    Rules rules;
    rules.lives = 1;
    Simulation simulation(rules);
    simulation.addPlayer(0);
    simulation.step();
    const EntityState ship = *shipOf(simulation.snapshot(), 0);
    place(simulation, EntityKind::BydoShot, NoOwner, ship.x, ship.y, 1000);

    simulation.step();

    CHECK(simulation.snapshot().gameOver == GameOutcome::Defeat);
    CHECK(simulation.snapshot().players[0].lives == 0);
    run(simulation, static_cast<int>(rules.respawnDelay) + 10);
    CHECK(!shipOf(simulation.snapshot(), 0));
    simulation.step();
    CHECK(!simulation.snapshot().gameOver);
}

// --- The movement shared with the client ---

TEST_CASE("moveShip() moves a ship exactly as the game does, to the bit", "[game]")
{
    Rules rules;
    Simulation simulation(rules);
    simulation.addPlayer(0);
    simulation.step();
    const EntityState start = *shipOf(simulation.snapshot(), 0);
    rtype::ecs::Position predicted{start.x, start.y};
    std::mt19937 random(3);
    std::uniform_int_distribution<int> buttons(0, 0x0F);

    // Long enough to hit every wall many times
    for (int tick = 0; tick < 3600; ++tick) {
        const PlayerInput input{static_cast<std::uint8_t>(buttons(random))};
        simulation.setInput(0, input);
        simulation.step();
        predicted = moveShip(predicted, input, rules);

        const std::optional<EntityState> ship = shipOf(simulation.snapshot(), 0);
        REQUIRE(ship);
        REQUIRE(ship->x == predicted.x);
        REQUIRE(ship->y == predicted.y);
    }
}

TEST_CASE("A ship goes at the speed of the rules, as fast in diagonal, and stays in the area", "[game]")
{
    const Rules rules;
    CHECK(shipVelocity(PlayerInput{PlayerInput::Right}, rules).x == rules.shipSpeed);
    CHECK(shipVelocity(PlayerInput{PlayerInput::Up | PlayerInput::Down}, rules).y == 0.0f);
    const auto diagonal = shipVelocity(PlayerInput{PlayerInput::Down | PlayerInput::Left}, rules);
    CHECK(std::hypot(diagonal.x, diagonal.y) == Approx(rules.shipSpeed));

    const rtype::ecs::Position corner =
        moveShip({5.0f, 5.0f}, PlayerInput{PlayerInput::Up | PlayerInput::Left}, rules);
    CHECK(corner.x == rules.shipHalfWidth);
    CHECK(corner.y == rules.shipHalfHeight);
}

// --- Projectiles ---

namespace
{

ProjectileDefinition pulse()
{
    return {"pulse", "models/pulse.glb", ProjectileCategory::Plasma,
            rtype::physics::Collider{rtype::physics::Aabb{{}, {8.0f, 4.0f, 0.0f}}, {}, false}};
}

} // namespace

TEST_CASE("A registry gives back the projectiles it was given, and refuses doubles and unknown ids", "[game]")
{
    ProjectileRegistry registry;
    registry.addDefinition(pulse());

    CHECK(registry.hasDefinition("pulse"));
    CHECK(registry.getDefinition("pulse").category == ProjectileCategory::Plasma);
    CHECK(registry.getDefinition("pulse").model == "models/pulse.glb");
    CHECK_THROWS_AS(registry.addDefinition(pulse()), std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition({}), std::invalid_argument);
    CHECK_THROWS_AS(registry.getDefinition("nope"), std::out_of_range);
}

TEST_CASE(
    "spawnProjectile() makes a projectile with its model, category and collider, which flies and leaves",
    "[game]")
{
    Simulation simulation{Rules{}};
    GameContext &context = simulation.context();
    context.projectiles.addDefinition(pulse());

    const rtype::ecs::entity_t entity =
        spawnProjectile(simulation.ecs(), context, "pulse", 0, 500.0f, 300.0f, 100.0f, 0.0f);

    rtype::ecs::Ecs &ecs = simulation.ecs();
    CHECK(ecs.getComponent<Projectile>(entity).category == ProjectileCategory::Plasma);
    CHECK(ecs.getComponent<Projectile>(entity).model == "models/pulse.glb");
    CHECK(ecs.getComponent<Kind>(entity).value == EntityKind::PlayerShot);
    CHECK(ecs.hasComponent<rtype::physics::Collider>(entity));
    CHECK_THROWS_AS(spawnProjectile(ecs, context, "nope", 0, 0.0f, 0.0f, 0.0f, 0.0f), std::out_of_range);

    simulation.step();
    CHECK(simulation.snapshot().entities.size() == 1);
    run(simulation, 30 * TickRate);
    CHECK(simulation.snapshot().entities.empty());
}

TEST_CASE("Each category moves its projectiles its own way", "[game]")
{
    const Rules rules;
    const rtype::physics::Velocity launch{1000.0f, 0.0f};

    const auto bullet = getProjectileVelocity(ProjectileCategory::Bullet, launch, 50, rules);
    CHECK(bullet.x == Approx(1000.0f));
    CHECK(bullet.y == Approx(0.0f));

    CHECK(getProjectileVelocity(ProjectileCategory::Laser, launch, 50, rules).x == Approx(1000.0f));

    const float early = getProjectileVelocity(ProjectileCategory::Missile, launch, 5, rules).x;
    const float late = getProjectileVelocity(ProjectileCategory::Missile, launch, 10000, rules).x;
    CHECK(early > 1000.0f);
    CHECK(late == Approx(1000.0f * rules.missileMaxBoost));

    const auto up = getProjectileVelocity(ProjectileCategory::Plasma, launch, 0, rules);
    const auto down =
        getProjectileVelocity(ProjectileCategory::Plasma, launch, rules.plasmaZigzagInterval, rules);
    CHECK(up.y == Approx(-rules.plasmaZigzagSpeed));
    CHECK(down.y == Approx(rules.plasmaZigzagSpeed));
    CHECK(up.x == Approx(1000.0f));
}

TEST_CASE("A missile in the game speeds up", "[game]")
{
    Simulation simulation{Rules{}};
    GameContext &context = simulation.context();
    ProjectileDefinition missile = pulse();
    missile.id = "missile";
    missile.category = ProjectileCategory::Missile;
    context.projectiles.addDefinition(missile);
    spawnProjectile(simulation.ecs(), context, "missile", 0, 500.0f, 300.0f, 100.0f, 0.0f);

    run(simulation, 1);
    const float first = simulation.snapshot().entities.at(0).velocityX;
    run(simulation, 20);
    CHECK(simulation.snapshot().entities.at(0).velocityX > first);
}

// --- Weapons ---

namespace
{

/// A weapon firing "pulse" every 5 ticks, a magazine of 3 shots, reloaded in 20 ticks.
WeaponDefinition blaster()
{
    return {"blaster", "pulse", ProjectileCategory::Laser, 1000.0f, 5, 0.0f, 3, 20};
}

/// Gives the weapon to the player 0, who holds Fire.
void arm(Simulation &simulation, const WeaponDefinition &weapon)
{
    simulation.context().projectiles.addDefinition(pulse());
    simulation.context().weapons.addDefinition(weapon);
    simulation.addPlayer(0);
    equipWeapon(simulation.ecs(), simulation.context(), *findPlayerShip(simulation.ecs(), 0), weapon.id);
    simulation.setInput(0, held(PlayerInput::Fire));
}

} // namespace

TEST_CASE("A weapon registry gives back the weapons, and refuses doubles, unknown ids and no fire interval",
          "[game]")
{
    WeaponRegistry registry;
    registry.addDefinition(blaster());

    CHECK(registry.hasDefinition("blaster"));
    CHECK(registry.getDefinition("blaster").magazineSize == 3);
    CHECK_THROWS_AS(registry.addDefinition(blaster()), std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition({}), std::invalid_argument);
    WeaponDefinition still = blaster();
    still.id = "still";
    still.fireInterval = 0;
    CHECK_THROWS_AS(registry.addDefinition(still), std::invalid_argument);
    CHECK_THROWS_AS(registry.getDefinition("nope"), std::out_of_range);
}

TEST_CASE("equipWeapon() and spawnWeaponShot() refuse an unknown weapon", "[game]")
{
    Simulation simulation;
    simulation.addPlayer(0);
    const auto ship = *findPlayerShip(simulation.ecs(), 0);
    CHECK_THROWS_AS(equipWeapon(simulation.ecs(), simulation.context(), ship, "nope"), std::out_of_range);
    CHECK_THROWS_AS(spawnWeaponShot(simulation.ecs(), simulation.context(), "nope", 0, 0.0f, 0.0f),
                    std::out_of_range);
}

TEST_CASE("An armed ship fires at the rate, the speed and the mechanic of its weapon", "[game]")
{
    WeaponDefinition weapon = blaster();
    weapon.magazineSize = 0;
    Simulation simulation;
    arm(simulation, weapon);

    simulation.step();
    const std::vector<EntityState> first = shotsOf(simulation.snapshot());
    REQUIRE(first.size() == 1);
    CHECK(first[0].velocityX == Approx(1000.0f));

    run(simulation, 4);
    CHECK(shotsOf(simulation.snapshot()).size() == 1);
    simulation.step();
    CHECK(shotsOf(simulation.snapshot()).size() == 2);

    auto &projectiles = simulation.ecs().cache<Projectile>();
    for (rtype::ecs::entity_t entity = 0; entity < projectiles.size(); ++entity) {
        if (projectiles.has(entity)) {
            CHECK(projectiles.get(entity).category == ProjectileCategory::Laser);
        }
    }
}

TEST_CASE("A weapon empties its magazine, reloads, then fires again", "[game]")
{
    Simulation simulation;
    arm(simulation, blaster());

    // 3 shots, 5 ticks apart: ticks 1, 6 and 11
    run(simulation, 11);
    CHECK(shotsOf(simulation.snapshot()).size() == 3);
    const auto ship = *findPlayerShip(simulation.ecs(), 0);
    CHECK(simulation.ecs().getComponent<Armed>(ship).ammo == 0);

    // Reloading: nothing is fired for reloadTicks
    run(simulation, 19);
    CHECK(shotsOf(simulation.snapshot()).size() == 3);
    simulation.step();
    CHECK(shotsOf(simulation.snapshot()).size() == 4);
    CHECK(simulation.ecs().getComponent<Armed>(ship).ammo == 2);
}

TEST_CASE("A weapon with InfiniteAmmo never runs dry nor reloads", "[game]")
{
    WeaponDefinition weapon = blaster();
    weapon.magazineSize = InfiniteAmmo;
    Simulation simulation;
    arm(simulation, weapon);

    // 20 shots, well over any magazine, with a reload time that would show
    run(simulation, 5 * 20);
    const auto ship = *findPlayerShip(simulation.ecs(), 0);
    CHECK(simulation.ecs().getComponent<Armed>(ship).reloadLeft == 0);
    CHECK(simulation.snapshot().entities.size() > 3);
}

TEST_CASE("A projectile vanishes once it has flown the range of its weapon", "[game]")
{
    WeaponDefinition weapon = blaster();
    weapon.magazineSize = 0;
    weapon.range = 100.0f;
    Simulation simulation;
    arm(simulation, weapon);
    simulation.step();
    simulation.setInput(0, {});
    const std::uint32_t id = shotsOf(simulation.snapshot())[0].id;

    // 1000 units per second: 100 units take 6 ticks
    run(simulation, 4);
    CHECK(shotsOf(simulation.snapshot()).size() == 1);
    run(simulation, 3);
    CHECK(shotsOf(simulation.snapshot()).empty());
    static_cast<void>(id);
}

namespace
{

ShipDefinition scoutShip()
{
    return {"bydo_scout",
            "models/bydo_scout.glb",
            {"sounds/scout_move.ogg", "sounds/scout_destruction.ogg", "sounds/scout_collision.ogg"}};
}

EnemyDefinition scout()
{
    return {"bydo_scout", "bydo_scout", 30, 240.0f, 720.0f, "blaster"};
}

} // namespace

TEST_CASE("An enemy registry gives back the enemies, and refuses doubles and unknown ids", "[game]")
{
    EnemyRegistry registry;
    registry.addDefinition(scout());

    CHECK(registry.hasDefinition("bydo_scout"));
    const EnemyDefinition &enemy = registry.getDefinition("bydo_scout");
    CHECK(enemy.ship == "bydo_scout");
    CHECK(enemy.health == 30);
    CHECK(enemy.speed == Approx(240.0f));
    CHECK(enemy.acceleration == Approx(720.0f));
    CHECK(enemy.weapon == "blaster");
    CHECK_FALSE(registry.hasDefinition("nope"));
    CHECK_THROWS_AS(registry.addDefinition(scout()), std::invalid_argument);
    CHECK_THROWS_AS(registry.getDefinition("nope"), std::out_of_range);
}

TEST_CASE("An enemy registry refuses an enemy without id, ship, health, speed or weapon, or a negative "
          "acceleration",
          "[game]")
{
    EnemyRegistry registry;
    EnemyDefinition enemy = scout();

    SECTION("no id")
    {
        enemy.id.clear();
    }
    SECTION("no ship")
    {
        enemy.ship.clear();
    }
    SECTION("no health")
    {
        enemy.health = 0;
    }
    SECTION("no speed")
    {
        enemy.speed = 0.0f;
    }
    SECTION("a negative acceleration")
    {
        enemy.acceleration = -1.0f;
    }
    SECTION("no weapon")
    {
        enemy.weapon.clear();
    }

    CHECK_THROWS_AS(registry.addDefinition(enemy), std::invalid_argument);
    CHECK_FALSE(registry.hasDefinition("bydo_scout"));
}

TEST_CASE("An enemy with no acceleration is accepted, and its ship and weapon are not looked up", "[game]")
{
    Simulation simulation;
    EnemyDefinition enemy = scout();
    enemy.acceleration = 0.0f;
    enemy.ship = "not_registered_yet";
    enemy.weapon = "not_registered_yet";

    simulation.context().enemies.addDefinition(enemy);

    CHECK(simulation.context().enemies.hasDefinition("bydo_scout"));
}

TEST_CASE("A ship registry gives back the ships with their sounds, and refuses doubles and unknown ids",
          "[game]")
{
    ShipRegistry registry;
    registry.addDefinition(scoutShip());

    CHECK(registry.hasDefinition("bydo_scout"));
    const ShipDefinition &ship = registry.getDefinition("bydo_scout");
    CHECK(ship.model == "models/bydo_scout.glb");
    CHECK(ship.sounds.move == "sounds/scout_move.ogg");
    CHECK(ship.sounds.destruction == "sounds/scout_destruction.ogg");
    CHECK(ship.sounds.collision == "sounds/scout_collision.ogg");
    CHECK_FALSE(registry.hasDefinition("nope"));
    CHECK_THROWS_AS(registry.addDefinition(scoutShip()), std::invalid_argument);
    CHECK_THROWS_AS(registry.getDefinition("nope"), std::out_of_range);
}

TEST_CASE("A ship registry refuses a ship without id or model, and accepts one without sounds", "[game]")
{
    ShipRegistry registry;
    ShipDefinition nameless = scoutShip();
    nameless.id.clear();
    ShipDefinition invisible = scoutShip();
    invisible.model.clear();

    CHECK_THROWS_AS(registry.addDefinition(nameless), std::invalid_argument);
    CHECK_THROWS_AS(registry.addDefinition(invisible), std::invalid_argument);
    registry.addDefinition({"silent", "models/silent.glb", {}});
    CHECK(registry.getDefinition("silent").sounds.move.empty());
}

namespace
{

EnemyRegistry scoutEnemies()
{
    EnemyRegistry enemies;
    enemies.addDefinition(scout());
    return enemies;
}

WaveDefinition randomWave()
{
    WaveDefinition wave;
    wave.name = "scouts_random";
    wave.label = "Scouts";
    wave.description = "Five scouts from the right";
    wave.difficulty = 2;
    wave.quantity = 5;
    wave.spawnInterval = 0.5f;
    wave.placement = WavePlacement::Random;
    wave.enemy = "bydo_scout";
    return wave;
}

WaveDefinition manualWave()
{
    WaveDefinition wave;
    wave.name = "scouts_line";
    wave.label = "Scout line";
    wave.quantity = 2;
    wave.spawnInterval = 0.0f;
    wave.placement = WavePlacement::Manual;
    wave.spawns = {{"bydo_scout", 2000.0f, 200.0f}, {"bydo_scout", 2000.0f, 800.0f}};
    return wave;
}

} // namespace

TEST_CASE("A wave registry gives back the waves of both placements, and refuses doubles and unknown names",
          "[game]")
{
    const EnemyRegistry enemies = scoutEnemies();
    WaveRegistry registry;
    registry.addDefinition(randomWave(), enemies);
    registry.addDefinition(manualWave(), enemies);

    const WaveDefinition &random = registry.getDefinition("scouts_random");
    CHECK(random.label == "Scouts");
    CHECK(random.difficulty == 2);
    CHECK(random.quantity == 5);
    CHECK(random.spawnInterval == Approx(0.5f));
    CHECK(random.placement == WavePlacement::Random);
    CHECK(random.area.minX == Approx(GameWidth));
    CHECK(random.area.maxY == Approx(GameHeight));
    const WaveDefinition &manual = registry.getDefinition("scouts_line");
    CHECK(manual.placement == WavePlacement::Manual);
    REQUIRE(manual.spawns.size() == 2);
    CHECK(manual.spawns[1].y == Approx(800.0f));
    CHECK_FALSE(registry.hasDefinition("nope"));
    CHECK_THROWS_AS(registry.addDefinition(randomWave(), enemies), std::invalid_argument);
    CHECK_THROWS_AS(registry.getDefinition("nope"), std::out_of_range);
}

TEST_CASE("A wave registry refuses a wave that is not valid", "[game]")
{
    const EnemyRegistry enemies = scoutEnemies();
    WaveRegistry registry;
    WaveDefinition wave = randomWave();

    SECTION("no name")
    {
        wave.name.clear();
    }
    SECTION("no enemy")
    {
        wave.quantity = 0;
    }
    SECTION("a difficulty of 0")
    {
        wave.difficulty = 0;
    }
    SECTION("a difficulty above 5")
    {
        wave.difficulty = MaxWaveDifficulty + 1;
    }
    SECTION("a negative interval")
    {
        wave.spawnInterval = -0.1f;
    }
    SECTION("a random wave without enemy")
    {
        wave.enemy.clear();
    }
    SECTION("a random wave with an unknown enemy")
    {
        wave.enemy = "nope";
    }
    SECTION("a random wave with spawns")
    {
        wave.spawns = manualWave().spawns;
    }
    SECTION("a random wave with an empty area")
    {
        wave.area.minY = 10.0f;
        wave.area.maxY = 5.0f;
    }
    SECTION("a manual wave with a quantity other than its spawns")
    {
        wave = manualWave();
        wave.quantity = 3;
    }
    SECTION("a manual wave with an unknown enemy")
    {
        wave = manualWave();
        wave.spawns[1].enemy = "nope";
    }
    SECTION("a manual wave with an enemy")
    {
        wave = manualWave();
        wave.enemy = "bydo_scout";
    }

    CHECK_THROWS_AS(registry.addDefinition(wave, enemies), std::invalid_argument);
    CHECK_FALSE(registry.hasDefinition(wave.name));
}

TEST_CASE("A game context keeps the waves", "[game]")
{
    Simulation simulation;
    simulation.context().enemies.addDefinition(scout());
    simulation.context().waves.addDefinition(randomWave(), simulation.context().enemies);

    CHECK(simulation.context().waves.hasDefinition("scouts_random"));
}
