#include "FakeVoiceChannel.hpp"
#include "rtype/voice/Backend.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::voice::Backend;
using rtype::voice::carriedByGame;

TEST_CASE("The game carries the voice when one of the two players is on it", "[voice]")
{
    STATIC_REQUIRE(carriedByGame(Backend::Game, Backend::Game));
    STATIC_REQUIRE(carriedByGame(Backend::Game, Backend::Discord));
    STATIC_REQUIRE(carriedByGame(Backend::Discord, Backend::Game));
}

TEST_CASE("The game never carries the voice between two players on Discord", "[voice]")
{
    STATIC_REQUIRE_FALSE(carriedByGame(Backend::Discord, Backend::Discord));
}

TEST_CASE("The game carries no voice from nor to a player without voice", "[voice]")
{
    for (const Backend other : {Backend::None, Backend::Game, Backend::Discord}) {
        CHECK_FALSE(carriedByGame(Backend::None, other));
        CHECK_FALSE(carriedByGame(other, Backend::None));
    }
}

TEST_CASE("In a game of two players on Discord and two on the game, each hears the three others once",
          "[voice]")
{
    // Slots 0 and 1 on Discord, 2 and 3 on the game: a voice goes through the game, or through Discord, never
    // both, never neither
    const Backend backends[] = {Backend::Discord, Backend::Discord, Backend::Game, Backend::Game};
    for (std::size_t from = 0; from < 4; ++from) {
        for (std::size_t to = 0; to < 4; ++to) {
            if (from == to) {
                continue;
            }
            const bool byGame = carriedByGame(backends[from], backends[to]);
            const bool byDiscord = backends[from] == Backend::Discord && backends[to] == Backend::Discord;
            CHECK(byGame != byDiscord);
        }
    }
}

TEST_CASE("A voice channel knows the other players of its game, not its own", "[voice]")
{
    testing::FakeVoiceChannel fake;
    rtype::voice::VoiceChannel &voice = fake;

    voice.join({.self = 1});
    voice.addParticipant(3);
    voice.addParticipant(1);
    voice.addParticipant(0);
    voice.setBackend(3, Backend::Discord);
    voice.setParticipantVolume(0, 5.0f);

    const std::vector<rtype::voice::Participant> players = voice.participants();
    REQUIRE(players.size() == 2);
    CHECK(players[0].slot == 0);
    CHECK(players[0].backend == Backend::None);
    CHECK(players[0].volume == rtype::voice::MaxVolume);
    CHECK(players[1].slot == 3);
    CHECK(players[1].backend == Backend::Discord);

    voice.leave();
    CHECK(voice.participants().empty());
}
