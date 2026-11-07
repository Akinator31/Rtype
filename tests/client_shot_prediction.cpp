// Tests of the shots of the player drawn in the time of its ship online (ShotPrediction,
// docs/architecture/client-online.md).

#include "Game/ShotPrediction.hpp"
#include "rtype/game/Constants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

using Catch::Approx;
using rtype::client::ShotPrediction;
using rtype::game::EntityKind;
using rtype::game::EntityState;
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
