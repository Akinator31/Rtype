// Tests of the shots of the player drawn in the time of its ship online (ShotPrediction,
// docs/architecture/client-online.md).

#include "Game/Prediction.hpp"
#include "Game/ShotPrediction.hpp"
#include "Instance/InputBuffer.hpp"
#include "rtype/game/Config/GameConfigLoader.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/Hitbox.hpp"
#include "rtype/game/Simulation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using Catch::Approx;
using rtype::client::ShotPrediction;
using rtype::game::EntityKind;
using rtype::game::EntityState;
using rtype::game::PlayerInput;
using rtype::game::Snapshot;

namespace
{

constexpr std::uint8_t Slot = 1;

/// A shot at (200, 300), flying right at 600 units per second.
EntityState shot(std::uint32_t id, std::uint8_t owner = Slot, EntityKind kind = EntityKind::PlayerShot)
{
    return {id, kind, owner, 200.0f, 300.0f, 600.0f, 0.0f, 0, 0};
}

Snapshot world()
{
    Snapshot snapshot;
    snapshot.tick = 100;
    snapshot.entities = {shot(1),
                         shot(2, Slot, EntityKind::PlayerChargedShot),
                         shot(3, 2),
                         {4, EntityKind::BydoShot, rtype::game::NoOwner, 200.0f, 300.0f, -600.0f, 0.0f, 0, 0},
                         {5, EntityKind::PlayerShip, Slot, 150.0f, 300.0f, 0.0f, 0.0f, 3, 0}};
    return snapshot;
}

std::optional<EntityState> find(const Snapshot &snapshot, std::uint32_t id)
{
    const auto found = std::ranges::find(snapshot.entities, id, &EntityState::id);
    if (found == snapshot.entities.end()) {
        return std::nullopt;
    }
    return *found;
}

} // namespace

TEST_CASE("The shots of the player are moved on to the tick of its ship", "[shot-prediction]")
{
    ShotPrediction shots;
    Snapshot snapshot = world();
    // Drawn at tick 100, the ship at tick 112: 12 ticks, 0.2 s, 120 units further
    shots.apply(snapshot, Slot, 100.0, 112.0);

    CHECK(find(snapshot, 1)->x == Approx(320.0f));
    CHECK(find(snapshot, 1)->y == Approx(300.0f));
    CHECK(find(snapshot, 2)->x == Approx(320.0f));
}

TEST_CASE("The rest of the world stays in the past", "[shot-prediction]")
{
    ShotPrediction shots;
    Snapshot snapshot = world();
    shots.apply(snapshot, Slot, 100.0, 112.0);

    // The shots of another player, a Bydo shot, and the ship itself, which the prediction places
    CHECK(find(snapshot, 3)->x == Approx(200.0f));
    CHECK(find(snapshot, 4)->x == Approx(200.0f));
    CHECK(find(snapshot, 5)->x == Approx(150.0f));
}

TEST_CASE("A shot never goes back, the ship behind the drawn tick", "[shot-prediction]")
{
    ShotPrediction shots;
    Snapshot snapshot = world();
    shots.apply(snapshot, Slot, 100.0, 98.0);
    CHECK(find(snapshot, 1)->x == Approx(200.0f));
}

TEST_CASE("A shot destroyed is drawn once where it ended, then no more", "[shot-prediction]")
{
    ShotPrediction shots;
    shots.destroyed(1, 105);

    // The ship past its end: where the game destroyed it, 5 ticks after the drawn tick, for its impact
    Snapshot first = world();
    shots.apply(first, Slot, 100.0, 112.0);
    REQUIRE(find(first, 1));
    CHECK(find(first, 1)->x == Approx(250.0f));
    // The others go on
    CHECK(find(first, 2)->x == Approx(320.0f));

    // Not through what it hit: gone, though the interpolation still has it
    Snapshot second = world();
    shots.apply(second, Slot, 101.0, 113.0);
    CHECK_FALSE(find(second, 1));
    CHECK(find(second, 2));
}

TEST_CASE("A shot destroyed later than the ship flies on until then", "[shot-prediction]")
{
    ShotPrediction shots;
    shots.destroyed(1, 120);

    Snapshot snapshot = world();
    shots.apply(snapshot, Slot, 100.0, 112.0);
    CHECK(find(snapshot, 1)->x == Approx(320.0f));
}

TEST_CASE("A destruction is forgotten once the drawn tick passes it", "[shot-prediction]")
{
    ShotPrediction shots;
    shots.destroyed(1, 105);
    Snapshot snapshot = world();
    shots.apply(snapshot, Slot, 100.0, 112.0);
    CHECK(shots.size() == 1);

    Snapshot later;
    shots.apply(later, Slot, 107.0, 119.0);
    CHECK(shots.size() == 0);
}

// --- Predicted shots ---

namespace
{

const rtype::game::ShotLaunch Launch{{300.0f, 400.0f}, {1200.0f, 0.0f}};

ShotPrediction armed()
{
    ShotPrediction shots;
    shots.arm(rtype::game::ProjectileCategory::Bullet, rtype::game::Rules{});
    return shots;
}

std::vector<EntityState> ownShots(const Snapshot &snapshot)
{
    std::vector<EntityState> shots;
    std::ranges::copy_if(snapshot.entities, std::back_inserter(shots),
                         [](const EntityState &entity) { return ShotPrediction::isShotOf(entity, Slot); });
    return shots;
}

} // namespace

TEST_CASE("A predicted shot is drawn from the tick it is fired at, along the path of the game",
          "[shot-prediction]")
{
    ShotPrediction shots = armed();
    shots.predict({{110, Launch}}, 100);

    // Not before it is fired
    Snapshot before;
    shots.apply(before, Slot, 100.0, 109.0);
    CHECK(ownShots(before).empty());

    Snapshot fired;
    shots.apply(fired, Slot, 100.0, 110.0);
    REQUIRE(ownShots(fired).size() == 1);
    const EntityState shot = ownShots(fired)[0];
    CHECK(shot.id >= ShotPrediction::FirstGhostId);
    CHECK(shot.x == 300.0f);
    CHECK(shot.y == 400.0f);

    // 6 ticks later, 6 steps of the game: 120 units
    Snapshot later;
    shots.apply(later, Slot, 101.0, 116.0);
    REQUIRE(ownShots(later).size() == 1);
    CHECK(ownShots(later)[0].id == shot.id);
    CHECK(ownShots(later)[0].x == Approx(420.0f));
}

TEST_CASE("A predicted shot the prediction no longer gives, or the game never sends, is dropped",
          "[shot-prediction]")
{
    ShotPrediction shots = armed();
    shots.predict({{110, Launch}, {112, Launch}}, 100);
    CHECK(shots.predicted() == 2);

    // A snapshot said the weapon was not ready at 110: no longer predicted
    shots.predict({{112, Launch}}, 104);
    CHECK(shots.predicted() == 1);

    // Tick 112 confirmed by the snapshots, but no shot of the game came for it
    shots.predict({}, 112 + ShotPrediction::Grace - 1);
    CHECK(shots.predicted() == 1);
    shots.predict({}, 112 + ShotPrediction::Grace);
    CHECK(shots.predicted() == 0);
}

TEST_CASE("The shot of the game takes the place of the predicted one, under its id, without a jump",
          "[shot-prediction]")
{
    ShotPrediction shots = armed();
    shots.predict({{110, Launch}}, 100);
    Snapshot first;
    shots.apply(first, Slot, 100.0, 114.0);
    const std::uint32_t ghost = ownShots(first).at(0).id;

    // The snapshot of tick 112 has it, 2 ticks old: fired at 110
    EntityState server = shot(77);
    server.x = 340.0f;
    server.velocityX = 1200.0f;
    server.age = 2;
    shots.received(server, 112);
    CHECK(shots.idOf(77) == ghost);
    CHECK(shots.idOf(78) == 78);

    // Drawn in the past by the interpolation, then 4 ticks later in the time of the ship
    Snapshot second;
    second.entities = {server};
    shots.apply(second, Slot, 112.0, 116.0);
    REQUIRE(ownShots(second).size() == 1);
    CHECK(ownShots(second)[0].id == ghost);
    CHECK(ownShots(second)[0].x == Approx(420.0f));

    // Destroyed at 118: drawn there once, then gone with the predicted shot
    shots.destroyed(77, 118);
    Snapshot third;
    third.entities = {server};
    shots.apply(third, Slot, 113.0, 120.0);
    REQUIRE(ownShots(third).size() == 1);
    CHECK(ownShots(third)[0].x == Approx(460.0f));
    Snapshot fourth;
    fourth.entities = {server};
    shots.apply(fourth, Slot, 114.0, 121.0);
    CHECK(ownShots(fourth).empty());
    CHECK(shots.predicted() == 0);
}

TEST_CASE("A predicted shot whose shot of the game leaves the world drawn is done", "[shot-prediction]")
{
    ShotPrediction shots = armed();
    shots.predict({{110, Launch}}, 100);
    EntityState server = shot(77);
    server.age = 0;
    shots.received(server, 110);

    Snapshot drawn;
    drawn.entities = {server};
    shots.apply(drawn, Slot, 110.0, 116.0);
    CHECK(ownShots(drawn).size() == 1);

    Snapshot gone;
    shots.apply(gone, Slot, 112.0, 118.0);
    CHECK(ownShots(gone).empty());
    CHECK(shots.predicted() == 0);
}

// --- Against the game ---

namespace
{

/// A game and its client, one tick at a time, as in client_prediction.cpp: the client stamps its inputs
/// `lead` ticks ahead, predicts its ship and its shots, they reach the game `up` ticks later, and the
/// snapshots, every 2 ticks, reach the client `down` ticks later. The client draws the latest snapshot, its
/// shots in the time of its ship.
struct Duel {
    rtype::game::Rules rules;
    rtype::game::GameConfig config;
    std::optional<rtype::game::Simulation> simulation;
    rtype::server::InputBuffer buffer;
    rtype::client::Prediction prediction{rtype::game::Rules{}};
    ShotPrediction shots;

    std::uint32_t lead = 4;
    std::uint32_t up = 2;
    std::uint32_t down = 2;
    std::size_t redundancy = 8;
    std::function<bool(std::uint32_t sent)> lost = [](std::uint32_t) { return false; };

    struct Input {
        std::uint32_t arrives;
        std::uint32_t stamp;
        std::vector<PlayerInput> inputs;
    };
    struct Report {
        std::uint32_t arrives;
        std::uint32_t tick;
        std::optional<rtype::ecs::Position> ship;
        rtype::game::WeaponState weapon;
        std::vector<EntityState> shots;
        std::vector<std::uint32_t> destroyed;
    };
    std::deque<Input> inputs;
    std::deque<Report> reports;
    std::deque<PlayerInput> history;
    std::uint32_t stamp = 0;
    Snapshot view;
    double latest = 0.0;

    /// The shots of the player in the game, by tick, then by id.
    std::map<std::uint32_t, std::map<std::uint32_t, EntityState>> game;
    std::set<std::uint32_t> known;
    /// What the client drew: the id, the tick of the ship, the position.
    struct Drawn {
        std::uint32_t id;
        std::uint32_t ship;
        float x;
        float y;
    };
    std::vector<Drawn> drawn;
    /// The shot of the game each predicted one became.
    std::map<std::uint32_t, std::uint32_t> became;

    explicit Duel(const std::string &weaponId)
    {
        const rtype::game::LoadedGameConfig loaded = rtype::game::loadGameConfig(RTYPE_TEST_CONFIG);
        REQUIRE(loaded.isValid());
        config = loaded.config;
        config.waves.clear();
        config.player.weapon = weaponId;
        simulation.emplace(rules, config);
        simulation->addPlayer(0);
        simulation->step();
        const rtype::game::WeaponDefinition &weapon = simulation->context().weapons.getDefinition(weaponId);
        const float halfWidth =
            rtype::game::halfSizeOf(
                simulation->context().projectiles.getDefinition(weapon.projectile).collider)
                .x;
        prediction.arm(weapon, halfWidth, 0);
        shots.arm(weapon.mechanic, rules);
        prediction.reconcile(shipOf(simulation->snapshot()), simulation->tick(), weaponOf());
    }

    [[nodiscard]] std::optional<rtype::ecs::Position> shipOf(const Snapshot &snapshot) const
    {
        for (const EntityState &entity : snapshot.entities) {
            if (entity.kind == EntityKind::PlayerShip && entity.owner == 0) {
                return rtype::ecs::Position{entity.x, entity.y};
            }
        }
        return std::nullopt;
    }

    rtype::game::WeaponState weaponOf()
    {
        const std::optional<rtype::game::Armed> armed = simulation->weaponOf(0);
        if (!armed) {
            return {};
        }
        return {static_cast<std::uint16_t>(armed->cooldown), static_cast<std::uint16_t>(armed->ammo),
                static_cast<std::uint16_t>(armed->reloadLeft)};
    }

    void tick(PlayerInput held)
    {
        const std::uint32_t now = simulation->tick();

        // The client: its input, stamped ahead, applied at once to its prediction
        stamp = now + lead;
        history.push_front(held);
        if (history.size() > redundancy) {
            history.pop_back();
        }
        prediction.apply(stamp, held);
        if (!lost(now)) {
            inputs.push_back({now + up, stamp, {history.begin(), history.end()}});
        }

        // The game
        while (!inputs.empty() && inputs.front().arrives <= now) {
            buffer.receive(inputs.front().stamp, inputs.front().inputs, simulation->tick());
            inputs.pop_front();
        }
        simulation->setInput(0, buffer.take(simulation->tick() + 1));
        simulation->step();
        const Snapshot &snapshot = simulation->snapshot();
        Report report{snapshot.tick + down, snapshot.tick, shipOf(snapshot), weaponOf(), {}, {}};
        for (const EntityState &entity : snapshot.entities) {
            if (ShotPrediction::isShotOf(entity, 0)) {
                game[snapshot.tick][entity.id] = entity;
                known.insert(entity.id);
                report.shots.push_back(entity);
            }
        }
        for (const rtype::game::DestroyedEntity &entity : snapshot.destroyed) {
            if (known.contains(entity.id)) {
                report.destroyed.push_back(entity.id);
            }
        }
        // The snapshots every 2 ticks; the destructions, reliable, every tick
        if (snapshot.tick % 2 != 0) {
            report.ship.reset();
            report.shots.clear();
        }
        reports.push_back(std::move(report));

        // The client: what arrived, then a frame
        while (!reports.empty() && reports.front().arrives <= simulation->tick()) {
            const Report &arrived = reports.front();
            for (const std::uint32_t id : arrived.destroyed) {
                shots.destroyed(id, arrived.tick);
            }
            if (arrived.tick % 2 == 0) {
                prediction.reconcile(arrived.ship, arrived.tick, arrived.weapon);
                for (const EntityState &shot : arrived.shots) {
                    shots.received(shot, arrived.tick);
                    if (shots.idOf(shot.id) != shot.id) {
                        became[shots.idOf(shot.id)] = shot.id;
                    }
                }
                view = Snapshot{};
                view.tick = arrived.tick;
                view.entities = arrived.shots;
                latest = arrived.tick;
            }
            reports.pop_front();
        }
        shots.predict(prediction.shots(), static_cast<std::uint32_t>(latest));
        Snapshot frame = view;
        shots.apply(frame, 0, latest, stamp);
        for (const EntityState &entity : frame.entities) {
            if (ShotPrediction::isShotOf(entity, 0)) {
                drawn.push_back({entity.id, stamp, entity.x, entity.y});
            }
        }
    }

    /// Fire held, moving and aiming every way.
    void play(int ticks)
    {
        for (int i = 0; i < ticks; ++i) {
            const auto buttons = static_cast<std::uint8_t>(((i / 7) % 128) | PlayerInput::Fire);
            tick(PlayerInput{buttons});
        }
    }

    /// The shot of the game a drawn shot is, and where the game has it at the tick of the ship.
    [[nodiscard]] std::optional<EntityState> truth(const Drawn &shot) const
    {
        const auto server = became.find(shot.id);
        const std::uint32_t id = server == became.end() ? shot.id : server->second;
        const auto tick = game.find(shot.ship);
        if (tick == game.end()) {
            return std::nullopt;
        }
        const auto found = tick->second.find(id);
        if (found == tick->second.end()) {
            return std::nullopt;
        }
        return found->second;
    }
};

} // namespace

TEST_CASE("Online, each shot of the player is drawn from the frame it fires, where the game will have it",
          "[shot-prediction]")
{
    const rtype::game::LoadedGameConfig loaded = rtype::game::loadGameConfig(RTYPE_TEST_CONFIG);
    REQUIRE(loaded.isValid());
    for (const rtype::game::WeaponDefinition &weapon : loaded.config.weapons) {
        CAPTURE(weapon.id);
        Duel duel(weapon.id);
        duel.play(600);

        // Every shot drawn was predicted: none of the game drawn under its own id, late
        std::size_t compared = 0;
        for (const Duel::Drawn &shot : duel.drawn) {
            REQUIRE(shot.id >= ShotPrediction::FirstGhostId);
            const std::optional<EntityState> truth = duel.truth(shot);
            if (!truth) {
                // A tick the game did not simulate yet, or the one it destroyed the shot at
                continue;
            }
            REQUIRE(shot.x == truth->x);
            REQUIRE(shot.y == truth->y);
            ++compared;
        }
        CHECK(compared > 50);

        // Each shot of the game was drawn at the tick it was fired at, at the frame the input was sent
        for (const auto &[tick, shots] : duel.game) {
            for (const auto &[id, shot] : shots) {
                // Fired, and in a snapshot the client received: the first even tick from then
                if (shot.age != 0 || static_cast<double>(tick + 2) > duel.latest) {
                    continue;
                }
                const auto ghost =
                    std::ranges::find_if(duel.became, [&](const auto &entry) { return entry.second == id; });
                REQUIRE(ghost != duel.became.end());
                CHECK(std::ranges::any_of(duel.drawn, [&](const Duel::Drawn &drawn) {
                    return drawn.id == ghost->first && drawn.ship == tick;
                }));
            }
        }
    }
}

TEST_CASE("Online, inputs lost without their copies: the shots drawn come back to the game's",
          "[shot-prediction]")
{
    Duel duel("player_cannon");
    duel.redundancy = 1;
    duel.lost = [](std::uint32_t sent) { return sent % 5 == 0; };
    duel.play(600);
    // Firing no more: what was predicted wrong is dropped, what the game fired is done once it is gone
    for (int i = 0; i < 240; ++i) {
        duel.tick(PlayerInput{});
    }
    CHECK(duel.shots.predicted() == 0);

    // At the end, nothing drawn twice: at most the shots the game still has
    Snapshot frame = duel.view;
    duel.shots.apply(frame, 0, duel.latest, duel.stamp);
    CHECK(ownShots(frame).size() <= duel.view.entities.size());
}
