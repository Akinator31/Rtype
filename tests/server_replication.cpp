// Tests of the inputs kept by the game for the ticks they are stamped with (InputBuffer,
// docs/protocol/game.md, section 9.2).

#include "Instance/InputBuffer.hpp"
#include "Instance/RecentDestructions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

using rtype::game::PlayerInput;
using rtype::server::InputBuffer;

namespace
{

const PlayerInput Up{PlayerInput::Up};
const PlayerInput Down{PlayerInput::Down};
const PlayerInput Fire{PlayerInput::Fire};

/// The inputs of the ticks from `first` to `last`, as the game applies them.
std::vector<PlayerInput> play(InputBuffer &buffer, std::uint32_t first, std::uint32_t last)
{
    std::vector<PlayerInput> applied;
    for (std::uint32_t tick = first; tick <= last; ++tick) {
        applied.push_back(buffer.take(tick));
    }
    return applied;
}

} // namespace

TEST_CASE("An input is applied at the tick it is stamped with, not when it arrives", "[replication]")
{
    InputBuffer buffer;
    // Received while the game is at tick 10, for the ticks 12 and 13
    buffer.receive(13, std::vector{Fire, Up}, 10);

    CHECK(buffer.take(11) == PlayerInput{});
    CHECK(buffer.take(12) == Up);
    CHECK(buffer.lastApplied() == 12);
    CHECK(buffer.take(13) == Fire);
    CHECK(buffer.lastApplied() == 13);
}

TEST_CASE("A tick without its input repeats the last one, until a silence empties it", "[replication]")
{
    InputBuffer buffer;
    buffer.receive(1, std::vector{Up}, 0);
    REQUIRE(buffer.take(1) == Up);

    // A lost datagram continues the movement
    const std::vector<PlayerInput> repeated = play(buffer, 2, 1 + InputBuffer::MaxSilence);
    CHECK(std::ranges::all_of(repeated, [](PlayerInput input) { return input == Up; }));
    CHECK(buffer.lastApplied() == 1);

    // A player who lost the connection stops
    CHECK(buffer.take(2 + InputBuffer::MaxSilence) == PlayerInput{});
}

TEST_CASE("The inputs sent again in the next datagrams fill the ticks of a lost one", "[replication]")
{
    InputBuffer buffer;
    // Each INPUT carries the input of its tick and of the ticks before it, newest first
    buffer.receive(1, std::vector{Up}, 0);
    REQUIRE(buffer.take(1) == Up);
    // The INPUT of tick 2 is lost; the one of tick 3 brings 3, 2 and 1 again
    buffer.receive(3, std::vector{Fire, Down, Up}, 1);

    CHECK(buffer.take(2) == Down);
    CHECK(buffer.take(3) == Fire);
}

TEST_CASE("Inputs too late, too far ahead, or received twice are ignored", "[replication]")
{
    InputBuffer buffer;

    // Too late: the ticks 5 and before are simulated
    buffer.receive(5, std::vector{Up, Up}, 5);
    CHECK(buffer.size() == 0);

    // Too far ahead: at most MaxAhead ticks
    buffer.receive(5 + InputBuffer::MaxAhead + 1, std::vector{Up}, 5);
    CHECK(buffer.size() == 0);
    buffer.receive(5 + InputBuffer::MaxAhead, std::vector{Fire}, 5);
    CHECK(buffer.size() == 1);

    // The same tick again keeps the first
    buffer.receive(7, std::vector{Up}, 5);
    buffer.receive(7, std::vector{Down}, 5);
    CHECK(buffer.take(6) == PlayerInput{});
    CHECK(buffer.take(7) == Up);
}

TEST_CASE("Inputs for tick 0 and below never wrap around", "[replication]")
{
    InputBuffer buffer;
    // An INPUT of tick 1 with more inputs than ticks: the decoder refuses it, the buffer stays safe anyway
    buffer.receive(1, std::vector{Up, Down, Fire}, 0);

    CHECK(buffer.size() == 1);
    CHECK(buffer.take(1) == Up);
}

TEST_CASE("The inputs of the ticks simulated are forgotten, and a reset forgets everything", "[replication]")
{
    InputBuffer buffer;
    buffer.receive(20, std::vector<PlayerInput>(8, Up), 10);
    REQUIRE(buffer.size() == 8);

    // Tick 15 taken: 13 to 15 applied or skipped, 16 to 20 kept
    (void)buffer.take(15);
    CHECK(buffer.size() == 5);

    buffer.reset();
    CHECK(buffer.size() == 0);
    CHECK(buffer.lastApplied() == 0);
    CHECK(buffer.take(16) == PlayerInput{});
}

TEST_CASE("The lead of the INPUT messages is measured, even for the ones ignored", "[replication]")
{
    InputBuffer buffer;
    CHECK_FALSE(buffer.takeLead());

    // At tick 10: one 3 ticks early, one just in time, the smallest is kept
    buffer.receive(13, std::vector{Up}, 10);
    buffer.receive(11, std::vector{Up}, 10);
    CHECK(buffer.takeLead() == 1);
    // Taken: the next ones start again
    CHECK_FALSE(buffer.takeLead());

    // Too late, or too far ahead: ignored, but measured, so that the client corrects its stamps
    buffer.receive(9, std::vector{Up}, 10);
    CHECK(buffer.takeLead() == -1);
    buffer.receive(10 + InputBuffer::MaxAhead + 5, std::vector{Up}, 10);
    CHECK(buffer.takeLead() == InputBuffer::MaxAhead + 5);
    // Only the two in time are kept
    CHECK(buffer.size() == 2);
}

// --- The destructions sent again ---

TEST_CASE("A destruction is sent again with the snapshots of the next Repeat ticks, then forgotten",
          "[replication]")
{
    using rtype::game::DestroyCause;
    using rtype::game::EntitiesDestroyedMessage;
    using rtype::server::RecentDestructions;

    RecentDestructions destructions;
    const EntitiesDestroyedMessage at10{10, {{7, DestroyCause::Killed}}};
    const EntitiesDestroyedMessage at11{11, {{8, DestroyCause::Removed}, {9, DestroyCause::Removed}}};
    destructions.add(10, {at10});
    destructions.add(11, {at11});
    destructions.add(12, {});

    // Not with the snapshot of their own tick: sent already
    CHECK(destructions.again(10).empty());
    CHECK(destructions.again(12) == std::vector{at10, at11});
    CHECK(destructions.again(10 + RecentDestructions::Repeat) == std::vector{at10, at11});
    CHECK(destructions.again(11 + RecentDestructions::Repeat) == std::vector{at11});
    CHECK(destructions.again(12 + RecentDestructions::Repeat).empty());

    destructions.add(12 + RecentDestructions::Repeat, {});
    CHECK(destructions.size() == 0);
}
