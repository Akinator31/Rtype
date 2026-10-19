// Tests of the Interpolation of the client: a world received 30 times per second, late, irregularly or not
// at all, drawn at 144 frames per second (docs/architecture/client-online.md, docs/protocol/game.md 9.3).

#include "Game/Interpolation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <vector>

using Catch::Approx;
using rtype::client::Interpolation;
using rtype::game::EntityState;

namespace
{

constexpr double Frame = 1.0 / 144.0;
/// Units per tick of the entity that moves: 600 per second, a ship.
constexpr float Step = 10.0f;

/// The world of the server at a tick: entity 1 moving right, and any other the test adds.
using WorldOf = std::function<std::map<std::uint32_t, EntityState>(std::uint32_t tick)>;

std::map<std::uint32_t, EntityState> moving(std::uint32_t tick)
{
    EntityState entity{1, rtype::game::EntityKind::PlayerShip, 0};
    entity.x = static_cast<float>(tick) * Step;
    entity.velocityX = Step * 60.0f;
    return {{1, entity}};
}

std::optional<EntityState> find(const rtype::game::Snapshot &snapshot, std::uint32_t id)
{
    for (const EntityState &entity : snapshot.entities) {
        if (entity.id == id) {
            return entity;
        }
    }
    return std::nullopt;
}

/// A server that sends a snapshot every 2 ticks, and a client that draws at 144 frames per second.
struct Network {
    Interpolation interpolation;
    WorldOf world = moving;
    /// Seconds a snapshot takes to arrive.
    std::function<double(std::uint32_t tick)> latency = [](std::uint32_t) { return 0.04; };
    /// Snapshots that never arrive.
    std::function<bool(std::uint32_t tick)> lost = [](std::uint32_t) { return false; };
    /// Server ticks per second of client time: 60, or a little more for a clock that drifts.
    double serverRate = 60.0;
    /// Ticks after which the server stops sending.
    std::uint32_t lastTick = 1000000;

    double now = 0.0;
    std::uint32_t nextSent = 2;
    std::vector<std::pair<double, std::uint32_t>> inFlight;

    /// Plays frames for `seconds`, calling `each` after each frame.
    void run(double seconds, const std::function<void()> &each = {})
    {
        const double end = now + seconds;
        while (now < end) {
            now += Frame;
            // Sent by the server since the previous frame
            while (nextSent <= lastTick && static_cast<double>(nextSent) / serverRate <= now) {
                if (!lost(nextSent)) {
                    inFlight.emplace_back(static_cast<double>(nextSent) / serverRate + latency(nextSent),
                                          nextSent);
                }
                nextSent += 2;
            }
            // Arrived: in the order they arrive, which may not be the order they were sent
            std::ranges::sort(inFlight);
            while (!inFlight.empty() && inFlight.front().first <= now) {
                const std::uint32_t tick = inFlight.front().second;
                interpolation.receive(tick, world(tick), {{0, 3, tick}});
                inFlight.erase(inFlight.begin());
            }
            interpolation.advance(Frame);
            if (each) {
                each();
            }
        }
    }

    [[nodiscard]] std::optional<float> x() const
    {
        const auto entity = find(interpolation.view(), 1);
        return entity ? std::optional(entity->x) : std::nullopt;
    }
};

/// The distance the entity goes in each frame, for `seconds`.
std::vector<float> steps(Network &network, double seconds)
{
    std::vector<float> distances;
    float previous = *network.x();
    network.run(seconds, [&] {
        const float current = *network.x();
        distances.push_back(current - previous);
        previous = current;
    });
    return distances;
}

/// What it should go in a frame: 600 units per second, at 144 frames per second.
constexpr float PerFrame = Step * 60.0f * static_cast<float>(Frame);

} // namespace

TEST_CASE("Nothing is drawn before the first world", "[interpolation]")
{
    Interpolation interpolation;
    interpolation.advance(Frame);

    CHECK_FALSE(interpolation.drawnTick());
    CHECK(interpolation.view().entities.empty());
}

TEST_CASE("The world is drawn 100 ms behind the newest snapshot", "[interpolation]")
{
    Network network;
    network.run(2.0);

    const double behind =
        static_cast<double>(network.interpolation.newestTick()) - *network.interpolation.drawnTick();
    CHECK(behind == Approx(Interpolation::Delay * 60.0).margin(2.0));
    // The entity is where the server had it at the drawn tick
    CHECK(*network.x() == Approx(*network.interpolation.drawnTick() * Step).margin(Step * 0.5));
}

TEST_CASE("An entity moves a little at every frame, not 30 times per second", "[interpolation]")
{
    Network network;
    network.run(1.0);

    const std::vector<float> distances = steps(network, 3.0);

    // Without interpolation, most frames would not move it, and some would move it 20 units at once
    for (const float distance : distances) {
        REQUIRE(distance == Approx(PerFrame).margin(PerFrame * 0.25));
    }
}

TEST_CASE("Snapshots arriving irregularly still move the entity smoothly", "[interpolation]")
{
    Network network;
    std::mt19937 random(7);
    std::uniform_real_distribution<double> jitter(0.02, 0.09);
    // From 20 to 90 ms: some even arrive after the next one, and are ignored
    std::map<std::uint32_t, double> latencies;
    network.latency = [&](std::uint32_t tick) {
        return latencies.try_emplace(tick, jitter(random)).first->second;
    };
    network.run(1.0);

    const std::vector<float> distances = steps(network, 5.0);

    for (const float distance : distances) {
        // Never backwards, never stopped, never a jump
        REQUIRE(distance > PerFrame * 0.3f);
        REQUIRE(distance < PerFrame * 2.0f);
    }
}

TEST_CASE("Lost snapshots are bridged: the entity goes on between the ones that arrive", "[interpolation]")
{
    Network network;
    // One snapshot in four lost, and sometimes two in a row
    network.lost = [](std::uint32_t tick) { return tick % 8 == 0 || tick % 30 == 2 || tick % 30 == 4; };
    network.run(1.0);

    const std::vector<float> distances = steps(network, 5.0);

    for (const float distance : distances) {
        REQUIRE(distance > PerFrame * 0.3f);
        REQUIRE(distance < PerFrame * 2.0f);
    }
}

TEST_CASE("Without snapshots, an entity goes on with its velocity for a while, then waits", "[interpolation]")
{
    Network network;
    network.lastTick = 120;
    // Sent at 2 s, arrived 40 ms later
    network.run(2.1);
    REQUIRE(network.interpolation.newestTick() == 120);
    const float last = 120.0f * Step;

    network.run(2.0);

    // 600 units per second for MaxExtrapolation seconds, then nothing more
    CHECK(*network.x() ==
          Approx(last + (Step * 60.0f * static_cast<float>(Interpolation::MaxExtrapolation))));
    const float waiting = *network.x();
    network.run(3.0);
    CHECK(*network.x() == waiting);
}

TEST_CASE("The drawn tick never goes back, even after a long silence", "[interpolation]")
{
    Network network;
    network.lastTick = 120;
    // The first drawn tick is before the first snapshot, below 0
    double previous = -std::numeric_limits<double>::infinity();
    bool backwards = false;
    network.run(10.0, [&] {
        // Nothing drawn before the first snapshot arrives
        if (const auto drawn = network.interpolation.drawnTick()) {
            backwards = backwards || *drawn < previous;
            previous = *drawn;
        }
    });

    CHECK_FALSE(backwards);
}

TEST_CASE("An entity appears and disappears when the drawn tick reaches it, not when it arrives",
          "[interpolation]")
{
    Network network;
    // Entity 2 lives from tick 20 to tick 40, when it is destroyed
    network.world = [](std::uint32_t tick) {
        auto entities = moving(tick);
        if (tick >= 20 && tick < 40) {
            entities.emplace(2, EntityState{2, rtype::game::EntityKind::BydoShooter, rtype::game::NoOwner});
        }
        return entities;
    };
    bool told = false;
    std::vector<std::pair<double, bool>> seen;
    network.run(3.0, [&] {
        // ENTITIES_DESTROYED of tick 40, which arrives with the snapshot of tick 40
        if (!told && network.interpolation.newestTick() >= 40) {
            network.interpolation.destroyed(2, 40);
            told = true;
        }
        if (const auto drawn = network.interpolation.drawnTick()) {
            seen.emplace_back(*drawn, find(network.interpolation.view(), 2).has_value());
        }
    });

    REQUIRE(told);
    for (const auto &[drawn, visible] : seen) {
        INFO("drawn tick " << drawn);
        if (drawn < 20.0 || drawn >= 40.0) {
            CHECK_FALSE(visible);
        } else {
            CHECK(visible);
        }
    }
}

TEST_CASE("A server clock that runs a little faster is followed without a jump", "[interpolation]")
{
    Network network;
    network.serverRate = 61.0;
    network.run(1.0);

    double worst = 0.0;
    network.run(10.0, [&] {
        const double behind =
            static_cast<double>(network.interpolation.newestTick()) - *network.interpolation.drawnTick();
        worst = std::max(worst, std::abs(behind - (Interpolation::Delay * 60.0)));
    });

    // Within a few ticks of the delay all along: the drawn tick speeds up a little instead of drifting away
    CHECK(worst < 4.0);
}

TEST_CASE("After a pause, the drawn tick jumps to the newest snapshots", "[interpolation]")
{
    Interpolation interpolation;
    interpolation.receive(10, moving(10), {});
    interpolation.advance(Frame);
    REQUIRE(*interpolation.drawnTick() == Approx(10.0 - (Interpolation::Delay * 60.0)));

    // Two seconds without a frame (a window dragged, a debugger), the snapshots kept coming
    for (std::uint32_t tick = 12; tick <= 130; tick += 2) {
        interpolation.receive(tick, moving(tick), {});
    }
    interpolation.advance(2.0);

    CHECK(*interpolation.drawnTick() == Approx(130.0 - (Interpolation::Delay * 60.0)));
    CHECK(interpolation.size() <= Interpolation::MaxWorlds);
}

TEST_CASE("An older world is ignored, the same tick again replaces it", "[interpolation]")
{
    Interpolation interpolation;
    interpolation.receive(20, moving(20), {});
    interpolation.receive(18, moving(100), {});
    CHECK(interpolation.newestTick() == 20);
    CHECK(interpolation.size() == 1);

    // The parts of a tick come one by one: the world of a tick grows
    auto more = moving(20);
    more.emplace(5, EntityState{5, rtype::game::EntityKind::PlayerShot, 0});
    interpolation.receive(20, more, {{0, 2, 50}});
    interpolation.advance(Frame);
    CHECK(interpolation.size() == 1);
    CHECK(interpolation.view().entities.size() == 2);
    CHECK(interpolation.view().players == std::vector<rtype::game::PlayerState>{{0, 2, 50}});
}
