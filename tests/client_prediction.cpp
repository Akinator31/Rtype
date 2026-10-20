// Tests of the Prediction of the ship of the player online, against the real rules: a Simulation of libs/game
// with the InputBuffer of the server, the inputs and the snapshots delayed, sometimes lost
// (docs/architecture/client-online.md).

#include "Game/Prediction.hpp"
#include "Instance/InputBuffer.hpp"
#include "rtype/game/Simulation.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <vector>

using Catch::Approx;
using rtype::client::Prediction;
using rtype::game::EntityKind;
using rtype::game::PlayerInput;

namespace
{

constexpr double Tick = 1.0 / 60.0;

std::optional<rtype::ecs::Position> shipOf(const rtype::game::Snapshot &snapshot)
{
    for (const rtype::game::EntityState &entity : snapshot.entities) {
        if (entity.kind == EntityKind::PlayerShip && entity.owner == 0) {
            return rtype::ecs::Position{entity.x, entity.y};
        }
    }
    return std::nullopt;
}

float distance(rtype::ecs::Position first, rtype::ecs::Position second)
{
    return std::hypot(first.x - second.x, first.y - second.y);
}

/// A game and its client, one tick at a time: the client stamps its inputs `lead` ticks ahead, they reach the
/// game `up` ticks later, and the snapshots, every 2 ticks, reach the client `down` ticks later.
struct Match {
    rtype::game::Simulation simulation{rtype::game::Rules{}};
    rtype::server::InputBuffer buffer;
    Prediction prediction{rtype::game::Rules{}};

    std::uint32_t lead = 4;
    std::uint32_t up = 2;
    std::uint32_t down = 2;
    /// Inputs per INPUT: 8 like the client, or 1 to see what a lost one costs.
    std::size_t redundancy = 8;
    /// INPUT messages that never arrive, by the tick they were sent at.
    std::function<bool(std::uint32_t sent)> lost = [](std::uint32_t) { return false; };

    struct Input {
        std::uint32_t arrives;
        std::uint32_t stamp;
        std::vector<PlayerInput> inputs;
    };
    struct Snapshot {
        std::uint32_t arrives;
        std::uint32_t tick;
        std::optional<rtype::ecs::Position> ship;
    };
    std::deque<Input> inputs;
    std::deque<Snapshot> snapshots;
    std::deque<PlayerInput> history;
    /// How far the prediction moved at each reconciliation: 0 when it guessed right.
    std::vector<float> corrections;

    Match()
    {
        simulation.addPlayer(0);
        simulation.step();
        prediction.reconcile(shipOf(simulation.snapshot()), simulation.tick());
    }

    /// One tick of both, the client holding `held`.
    void tick(PlayerInput held)
    {
        const std::uint32_t now = simulation.tick();

        // The client: its input, stamped ahead, applied at once to its prediction
        const std::uint32_t stamp = now + lead;
        history.push_front(held);
        if (history.size() > redundancy) {
            history.pop_back();
        }
        prediction.apply(stamp, held);
        if (!lost(now)) {
            inputs.push_back({now + up, stamp, {history.begin(), history.end()}});
        }

        // The game: the inputs arrived, then one tick
        while (!inputs.empty() && inputs.front().arrives <= now) {
            buffer.receive(inputs.front().stamp, inputs.front().inputs, simulation.tick());
            inputs.pop_front();
        }
        simulation.setInput(0, buffer.take(simulation.tick() + 1));
        simulation.step();
        if (simulation.tick() % 2 == 0) {
            snapshots.push_back({simulation.tick() + down, simulation.tick(), shipOf(simulation.snapshot())});
        }

        // The client: the snapshots arrived
        while (!snapshots.empty() && snapshots.front().arrives <= simulation.tick()) {
            const std::optional<rtype::ecs::Position> before = prediction.predicted();
            prediction.reconcile(snapshots.front().ship, snapshots.front().tick);
            if (before && prediction.predicted()) {
                corrections.push_back(distance(*before, *prediction.predicted()));
            }
            snapshots.pop_front();
        }
        prediction.advance(Tick);
    }

    /// Ticks with inputs that change often, over every direction.
    void play(int ticks)
    {
        for (int i = 0; i < ticks; ++i) {
            tick(PlayerInput{static_cast<std::uint8_t>((i / 7) % 16)});
        }
    }

    [[nodiscard]] rtype::ecs::Position server() const { return *shipOf(simulation.snapshot()); }
};

} // namespace

TEST_CASE("Nothing lost: the prediction is never corrected", "[prediction]")
{
    Match match;
    match.play(600);

    REQUIRE(match.corrections.size() > 250);
    for (const float correction : match.corrections) {
        REQUIRE(correction == 0.0f);
    }
    // And what it draws is what it predicts: no correction fading out
    CHECK(match.prediction.position()->x == match.prediction.predicted()->x);
}

TEST_CASE("The ship answers the controls at once, ahead of the game", "[prediction]")
{
    Match match;
    match.play(30);
    const rtype::ecs::Position start = *match.prediction.position();
    const rtype::ecs::Position gameStart = match.server();

    // Right pressed: the predicted ship moves on this very tick, the game's only once the input arrives
    PlayerInput right;
    right.set(PlayerInput::Right, true);
    match.tick(right);

    CHECK(match.prediction.position()->x > start.x);
    CHECK(match.server().x == gameStart.x);
}

TEST_CASE("The prediction lands where the game will be once it has the inputs", "[prediction]")
{
    Match match;
    match.play(300);
    const rtype::ecs::Position predicted = *match.prediction.predicted();

    // Nothing pressed until the inputs sent have all been applied by the game
    for (std::uint32_t i = 0; i < match.lead + match.up + match.down + 4; ++i) {
        match.tick(PlayerInput{});
    }

    // The game went where the prediction was, then did not move
    CHECK(match.server().x == Approx(predicted.x));
    CHECK(match.server().y == Approx(predicted.y));
    CHECK(match.prediction.predicted()->x == match.server().x);
}

TEST_CASE("A lost INPUT costs nothing: the next ones carry its inputs again", "[prediction]")
{
    Match match;
    // One message in four lost, never 8 in a row
    match.lost = [](std::uint32_t sent) { return sent % 4 == 0; };
    match.play(600);

    for (const float correction : match.corrections) {
        REQUIRE(correction == 0.0f);
    }
}

TEST_CASE("Without redundancy, a lost input is corrected smoothly, not teleported", "[prediction]")
{
    Match match;
    match.redundancy = 1;
    match.lost = [](std::uint32_t sent) { return sent % 10 == 0; };
    match.play(300);

    // Some guesses were wrong: the game repeated the previous input instead of the lost one
    const float worst = *std::ranges::max_element(match.corrections);
    CHECK(worst > 0.0f);

    // A correction is drawn as a gap that fades out
    PlayerInput up;
    up.set(PlayerInput::Up, true);
    bool smoothed = false;
    for (int i = 0; i < 200 && !smoothed; ++i) {
        match.tick(i % 3 == 0 ? up : PlayerInput{});
        const rtype::ecs::Position drawn = *match.prediction.position();
        smoothed = distance(drawn, *match.prediction.predicted()) > 0.01f;
    }
    CHECK(smoothed);
    // ...and has faded out a few frames later
    for (int i = 0; i < 30; ++i) {
        match.prediction.advance(Tick);
    }
    CHECK(distance(*match.prediction.position(), *match.prediction.predicted()) < 0.01f);
}

TEST_CASE("The walls are predicted as the game has them", "[prediction]")
{
    Match match;
    PlayerInput corner;
    corner.set(PlayerInput::Up, true);
    corner.set(PlayerInput::Left, true);
    for (int i = 0; i < 240; ++i) {
        match.tick(corner);
    }

    const rtype::game::Rules rules {};
    CHECK(match.prediction.position()->x == rules.shipHalfWidth);
    CHECK(match.prediction.position()->y == rules.shipHalfHeight);
    for (const float correction : match.corrections) {
        REQUIRE(correction == 0.0f);
    }
}

TEST_CASE("Without a ship, nothing is predicted; when it comes back, it is where the game puts it",
          "[prediction]")
{
    Prediction prediction(rtype::game::Rules{});
    prediction.reconcile(rtype::ecs::Position{200.0f, 300.0f}, 10);
    REQUIRE(prediction.position());

    // Destroyed: the game decides when it comes back
    prediction.reconcile(std::nullopt, 12);
    CHECK_FALSE(prediction.position());
    prediction.apply(14, PlayerInput{PlayerInput::Right});
    CHECK_FALSE(prediction.position());

    // Back elsewhere: drawn there at once, not glided to
    prediction.reconcile(rtype::ecs::Position{200.0f, 800.0f}, 100);
    REQUIRE(prediction.position());
    CHECK(prediction.position()->y == 800.0f);
}

TEST_CASE("A correction too large is applied at once", "[prediction]")
{
    Prediction prediction(rtype::game::Rules{});
    prediction.reconcile(rtype::ecs::Position{200.0f, 300.0f}, 10);

    prediction.reconcile(rtype::ecs::Position{200.0f + Prediction::MaxSmoothed * 2.0f, 300.0f}, 12);

    CHECK(prediction.position()->x == 200.0f + Prediction::MaxSmoothed * 2.0f);
}

TEST_CASE("A tick skipped when stamping repeats the previous input, as the game does", "[prediction]")
{
    const rtype::game::Rules rules {};
    Prediction prediction(rules);
    prediction.reconcile(rtype::ecs::Position{500.0f, 500.0f}, 10);
    PlayerInput right;
    right.set(PlayerInput::Right, true);

    prediction.apply(11, right);
    // The stamp jumps from 11 to 14: the game applies Right at 12 and 13 too
    prediction.apply(14, PlayerInput{});

    CHECK(prediction.pending() == 4);
    CHECK(prediction.predicted()->x == Approx(500.0f + (3.0f * rules.shipSpeed / 60.0f)));
    // The same tick again is ignored
    prediction.apply(14, right);
    CHECK(prediction.pending() == 4);
}
