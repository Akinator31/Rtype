// Tests of the voice of a game carried by Discord (rtype::discord::DiscordVoice) against the fake call of
// tests/helpers/FakeSocialSdk.hpp: the lobby joined with the secret of the game, its members matched to the
// slots, their voices played through the playback of the game
// (docs/architecture/voice.md#the-voice-of-discord).

#include "FakeSocialSdk.hpp"
#include "FakeVoiceDevices.hpp"
#include "rtype/discord/DiscordVoice.hpp"
#include "rtype/voice/Format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

using rtype::discord::CallStatus;
using rtype::discord::CallTalkMode;
using rtype::discord::DiscordVoice;

namespace
{

constexpr std::uint32_t Self = 7;
constexpr std::uint32_t Bob = 8;
constexpr std::uint64_t BobOnDiscord = 1008;

/// A player of slot 0 on Discord, Bob in slot 1, the call connected.
struct Game {
    testing::FakeSocialSdk sdk;
    testing::FakePlayback playback;
    DiscordVoice voice{sdk.call, playback, "secret", Self};

    explicit Game(bool connect = true)
    {
        voice.addParticipant(1);
        voice.setPlayerId(1, Bob);
        voice.join({.self = 0});
        if (connect) {
            sdk.call.addMember(BobOnDiscord, Bob);
            sdk.call.connect();
            sdk.poll();
            voice.poll();
        }
    }
};

std::vector<std::int16_t> tone(std::size_t samples, double frequency, int rate, std::size_t channels = 1)
{
    std::vector<std::int16_t> result;
    for (std::size_t i = 0; i < samples; ++i) {
        const auto value = static_cast<std::int16_t>(
            std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(i) / rate) * 8000.0);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            result.push_back(value);
        }
    }
    return result;
}

} // namespace

TEST_CASE("The secret of a Discord lobby is the hexadecimal of the 32 bytes of the game", "[discord][voice]")
{
    std::array<std::byte, 32> secret{};
    secret[0] = std::byte{0xAB};
    secret[31] = std::byte{0x01};
    const std::string text = rtype::discord::lobbySecret(secret);
    CHECK(text.size() == 64);
    CHECK(text.starts_with("ab00"));
    CHECK(text.ends_with("0001"));
}

TEST_CASE("Joining the voice of Discord joins the lobby of the game with the id of the player",
          "[discord][voice]")
{
    Game game(false);
    REQUIRE(game.sdk.call.secrets == std::vector<std::string>{"secret"});
    CHECK(game.sdk.call.metadata.at(0) == std::map<std::string, std::string>{{"player", "7"}});
    // Muted by default, as the voice of the game; push-to-talk until told otherwise
    CHECK(game.sdk.call.selfMuted);
    CHECK(game.sdk.call.mode == CallTalkMode::PushToTalk);
    CHECK_FALSE(game.voice.connected());
    // No stream before the call connects: the voice of the game may still use them
    game.voice.poll();
    CHECK_FALSE(game.playback.streams[1]);

    game.sdk.call.connect();
    game.sdk.poll();
    CHECK(game.voice.connected());
    game.voice.poll();
    CHECK(game.playback.streams[1]);

    // Leaving ends the call, and closes the streams
    game.voice.leave();
    CHECK(game.sdk.call.ends == 1);
    CHECK(game.sdk.call.status() == CallStatus::Idle);
    CHECK_FALSE(game.playback.streams[1]);
}

TEST_CASE("The voice of a member of the lobby comes out of the stream of the slot of its player",
          "[discord][voice]")
{
    Game game;
    const std::vector<std::int16_t> voice = tone(960, 440.0, 48000);
    game.sdk.call.talk(BobOnDiscord, voice);
    // On the audio thread of Discord: played at the next poll of the game loop
    CHECK(game.playback.played[1].empty());
    game.voice.poll();
    CHECK(game.playback.played[1] == voice);

    // A member matched to no player, as one kicked who kept the secret: not heard
    game.sdk.call.addMember(1666, 99);
    game.voice.poll();
    game.sdk.call.talk(1666, voice);
    game.voice.poll();
    for (const std::vector<std::int16_t> &played : {game.playback.played[0], game.playback.played[2]}) {
        CHECK(played.empty());
    }
    CHECK(game.playback.played[1].size() == voice.size());

    // Bob leaves the game: his voice stops
    game.voice.removeParticipant(1);
    game.sdk.call.talk(BobOnDiscord, voice);
    game.voice.poll();
    CHECK_FALSE(game.playback.streams[1]);
    CHECK(game.playback.played[1].size() == voice.size());
}

TEST_CASE("A voice of Discord at another rate or in stereo is played at 48 kHz mono", "[discord][voice]")
{
    Game game;
    // 20 ms in stereo at 48 kHz: 960 samples once mixed down
    game.sdk.call.talk(BobOnDiscord, tone(960, 440.0, 48000, 2), 48000, 2);
    game.voice.poll();
    CHECK(game.playback.played[1] == tone(960, 440.0, 48000));

    // 20 ms at 24 kHz: twice as many samples, the same tone
    game.playback.played[1].clear();
    game.sdk.call.talk(BobOnDiscord, tone(480, 440.0, 24000), 24000, 1);
    game.voice.poll();
    const std::vector<std::int16_t> &played = game.playback.played[1];
    REQUIRE(played.size() == 960);
    const std::vector<std::int16_t> expected = tone(960, 440.0, 48000);
    // Linear between the samples of 24 kHz, two samples late: within a few percent of the tone at 48 kHz
    std::size_t far = 0;
    for (std::size_t i = 2; i < played.size(); ++i) {
        far += std::abs(played[i] - expected[i - 2]) >= 400 ? 1 : 0;
    }
    CHECK(far == 0);

    // A rate the game does not take is dropped
    game.playback.played[1].clear();
    game.sdk.call.talk(BobOnDiscord, tone(100, 440.0, 4000), 4000, 1);
    game.voice.poll();
    CHECK(game.playback.played[1].empty());
}

TEST_CASE("A player muted by me, by an administrator, or me deafened, is not heard through Discord",
          "[discord][voice]")
{
    Game game;
    const std::vector<std::int16_t> voice = tone(960, 440.0, 48000);
    game.sdk.call.others.at(0).speaking = true;
    game.voice.poll();
    CHECK(game.voice.participants().at(0).speaking);

    game.voice.setParticipantMute(1, true);
    game.sdk.call.talk(BobOnDiscord, voice);
    game.voice.poll();
    CHECK(game.playback.played[1].empty());
    CHECK(game.playback.volume[1] == 0.0f);
    CHECK_FALSE(game.voice.participants().at(0).speaking);
    game.voice.setParticipantMute(1, false);
    CHECK(game.playback.volume[1] == 1.0f);

    game.voice.setStatus(1, {.mutedByAdmin = true});
    game.sdk.call.talk(BobOnDiscord, voice);
    game.voice.poll();
    CHECK(game.playback.played[1].empty());
    game.voice.setStatus(1, {});

    game.voice.setDeafened(true);
    CHECK(game.sdk.call.selfDeafened);
    game.sdk.call.talk(BobOnDiscord, voice);
    game.voice.poll();
    CHECK(game.playback.played[1].empty());
    game.voice.setDeafened(false);

    // The volumes of the player and of every voice, as for the voice of the game
    game.voice.setParticipantVolume(1, 0.5f);
    game.voice.setOutputVolume(1.5f);
    CHECK(game.playback.volume[1] == 0.75f);
    game.sdk.call.talk(BobOnDiscord, voice);
    game.voice.poll();
    CHECK(game.playback.played[1] == voice);
}

TEST_CASE("How the player talks is given to the call of Discord", "[discord][voice]")
{
    Game game;
    game.voice.setSelfMute(false);
    CHECK_FALSE(game.sdk.call.selfMuted);

    // Push-to-talk: the key held
    game.voice.setTalking(true);
    CHECK(game.sdk.call.pushToTalk);
    game.voice.setTalking(false);
    CHECK_FALSE(game.sdk.call.pushToTalk);

    // The voice activity of Discord, at the threshold of the player
    game.voice.setVoiceActivity(-45.0f);
    CHECK(game.sdk.call.mode == CallTalkMode::VoiceActivity);
    CHECK(game.sdk.call.threshold == -45.0f);
    game.voice.setVoiceActivity(std::nullopt);
    CHECK(game.sdk.call.mode == CallTalkMode::PushToTalk);

    game.voice.setInputVolume(1.8f);
    CHECK(game.sdk.call.inputVolume == 1.0f);
    game.voice.setProcessing({.noiseSuppression = true, .gainControl = false});
    CHECK(game.sdk.call.noiseSuppression);
    CHECK_FALSE(game.sdk.call.gainControl);

    // Transmitting as Discord hears it, and never muted
    game.sdk.call.selfSpeaking = true;
    CHECK(game.voice.transmitting());
    game.voice.setSelfMute(true);
    CHECK_FALSE(game.voice.transmitting());
}

TEST_CASE("A call of Discord that fails, or drops, says so, and closes the streams", "[discord][voice]")
{
    // Discord not connected: failed at once
    {
        testing::FakeSocialSdk sdk;
        sdk.call.discordConnected = false;
        testing::FakePlayback playback;
        DiscordVoice voice(sdk.call, playback, "secret", Self);
        voice.join({.self = 0});
        CHECK(voice.failed());
    }
    Game game;
    CHECK(game.playback.streams[1]);
    game.sdk.call.fail();
    game.sdk.poll();
    CHECK(game.voice.failed());
    game.voice.poll();
    CHECK_FALSE(game.playback.streams[1]);
}
