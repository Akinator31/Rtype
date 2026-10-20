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

TEST_CASE("spawnProjectile() makes a projectile with its model, category and collider, which flies and leaves",
          "[game]")
{
    Simulation simulation{Rules{}};
    GameContext &context = simulation.context();
    context.projectiles.addDefinition(pulse());

    const rtype::ecs::entity_t entity = spawnProjectile(simulation.ecs(), context, "pulse", 0, 500.0f, 300.0f,
                                                        100.0f, 0.0f);

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
    const auto down = getProjectileVelocity(ProjectileCategory::Plasma, launch, rules.plasmaZigzagInterval, rules);
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
