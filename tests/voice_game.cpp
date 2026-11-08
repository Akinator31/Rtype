#include "helpers/FakeVoiceDevices.hpp"
#include "rtype/voice/GameVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using namespace std::chrono_literals;
using rtype::voice::Backend;
using rtype::voice::FrameSamples;
using rtype::voice::GameVoice;
using rtype::voice::SampleRate;
using testing::FakeMicrophone;
using testing::FakePlayback;
using testing::FakeTransport;

namespace
{

using Frame = std::array<std::int16_t, FrameSamples>;

/// A voice-like sound, frame after frame: a pitch of 180 Hz with its harmonics
struct Voice {
    double phase = 0.0;

    Frame next()
    {
        Frame frame{};
        for (std::int16_t &sample : frame) {
            phase += 2.0 * std::numbers::pi * 180.0 / SampleRate;
            double value = 0.0;
            for (int harmonic = 1; harmonic <= 8; ++harmonic) {
                value += std::sin(harmonic * phase) / harmonic;
            }
            sample = static_cast<std::int16_t>(value * 4000.0);
        }
        return frame;
    }
};

double rms(const std::vector<std::int16_t> &samples, std::size_t from)
{
    double sum = 0.0;
    for (std::size_t i = from; i < samples.size(); ++i) {
        sum += static_cast<double>(samples[i]) * samples[i];
    }
    return samples.size() > from ? std::sqrt(sum / static_cast<double>(samples.size() - from)) : 0.0;
}

/// A player of a game of two: its devices and its voice
struct Player {
    FakeTransport transport;
    FakeMicrophone microphone;
    FakePlayback playback;
    GameVoice voice{transport, microphone, playback};

    Player(rtype::voice::Slot self, rtype::voice::Slot other)
    {
        voice.addParticipant(other);
        voice.setBackend(other, Backend::Game);
        voice.join({.self = self});
    }

    void talk()
    {
        voice.setSelfMute(false);
        voice.setTalking(true);
    }
};

const GameVoice::Clock::time_point Start = GameVoice::Clock::now();

} // namespace

TEST_CASE("Muted by default, the microphone stays closed; unmuted, a frame goes out every 20 ms", "[voice]")
{
    Player alice(0, 1);
    Voice voice;
    CHECK(alice.voice.selfMuted());
    CHECK_FALSE(alice.voice.microphoneOpen());
    alice.voice.setTalking(true);
    alice.microphone.say(voice.next());
    alice.voice.poll(Start);
    CHECK(alice.transport.sent.empty());

    alice.voice.setSelfMute(false);
    CHECK(alice.microphone.isOpen);
    for (int i = 0; i < 10; ++i) {
        alice.microphone.say(voice.next());
        alice.voice.poll(Start);
    }
    REQUIRE(alice.transport.sent.size() == 10);
    for (std::uint16_t i = 0; i < 10; ++i) {
        CHECK(alice.transport.sent[i].sequence == i);
        CHECK_FALSE(alice.transport.sent[i].last);
        CHECK(alice.transport.sent[i].bytes.size() <= rtype::voice::MaxFrameBytes);
    }
    // Half a frame waits for the rest of it
    alice.microphone.say(std::vector<std::int16_t>(FrameSamples / 2, 100));
    alice.voice.poll(Start);
    CHECK(alice.voice.sentFrames() == 10);

    alice.voice.setSelfMute(true);
    CHECK_FALSE(alice.microphone.isOpen);
}

TEST_CASE("The voice captured by a player comes out of the stream of its slot on the other one", "[voice]")
{
    Player alice(0, 1);
    Player bob(1, 0);
    alice.talk();
    CHECK(bob.playback.streams[0]);
    CHECK(bob.playback.volume[0] == rtype::voice::DefaultVolume);

    Voice voice;
    std::vector<std::int16_t> said;
    auto now = Start;
    bool speaking = false;
    for (int i = 0; i < 50; ++i) {
        const Frame frame = voice.next();
        said.insert(said.end(), frame.begin(), frame.end());
        alice.microphone.say(frame);
        alice.voice.poll(now);
        bob.transport.deliver(alice.transport, 0);
        bob.voice.poll(now);
        speaking = speaking || bob.voice.participants().at(0).speaking;
        now += 20ms;
    }
    CHECK(speaking);
    // 40 ms in the jitter buffer: every frame but the last two played
    const std::vector<std::int16_t> &heard = bob.playback.played[0];
    CHECK(heard.size() == 48 * FrameSamples);
    // The same voice, as loud, past the first frames where the codec settles
    const double level = 20.0 * std::log10(rms(heard, 10 * FrameSamples) / rms(said, 10 * FrameSamples));
    CHECK(std::abs(level) < 3.0);
    // Alice never hears herself, nor plays anything of hers
    CHECK(alice.playback.played[1].empty());

    // Quiet for a while: Bob sees her stop
    for (int i = 0; i < 20; ++i) {
        bob.voice.poll(now);
        now += 20ms;
    }
    CHECK_FALSE(bob.voice.participants().at(0).speaking);
}

TEST_CASE("Silence is not sent, and the end of a talk spurt is told by its last frame", "[voice]")
{
    Player alice(0, 1);
    alice.talk();
    const Frame silence{};
    for (int i = 0; i < 50; ++i) {
        alice.microphone.say(silence);
        alice.voice.poll(Start);
    }
    // Opus waits 200 ms of silence before it stops: 10 frames at most
    CHECK(alice.transport.sent.size() <= 10);

    Voice voice;
    alice.transport.sent.clear();
    for (int i = 0; i < 5; ++i) {
        alice.microphone.say(voice.next());
        alice.voice.poll(Start);
    }
    REQUIRE(alice.transport.sent.size() == 5);
    const std::uint16_t sequence = alice.transport.sent.back().sequence;

    alice.voice.setTalking(false);
    alice.microphone.say(voice.next());
    alice.microphone.say(voice.next());
    alice.voice.poll(Start);
    REQUIRE(alice.transport.sent.size() == 6);
    CHECK(alice.transport.sent.back().last);
    CHECK(alice.transport.sent.back().sequence == sequence + 1);
}

TEST_CASE("A player is heard through the game unless it has no voice, is muted, or is gone", "[voice]")
{
    Player alice(0, 1);
    Player bob(1, 0);
    alice.talk();
    Voice voice;
    auto now = Start;
    const auto talkFor = [&](int frames) {
        bob.playback.played[0].clear();
        for (int i = 0; i < frames; ++i) {
            alice.microphone.say(voice.next());
            alice.voice.poll(now);
            bob.transport.deliver(alice.transport, 0);
            bob.voice.poll(now);
            now += 20ms;
        }
        // The spurt played out
        for (int i = 0; i < 10; ++i) {
            bob.voice.poll(now);
            now += 20ms;
        }
        return bob.playback.played[0].size();
    };

    CHECK(talkFor(10) > 0);

    bob.voice.setBackend(0, Backend::None);
    CHECK(talkFor(10) == 0);
    // On Discord, Bob being on the game: through the game
    bob.voice.setBackend(0, Backend::Discord);
    CHECK(talkFor(10) > 0);

    bob.voice.setParticipantMute(0, true);
    CHECK(bob.playback.volume[0] == 0.0f);
    CHECK(talkFor(10) == 0);
    bob.voice.setParticipantMute(0, false);
    bob.voice.setParticipantVolume(0, 5.0f);
    CHECK(bob.playback.volume[0] == rtype::voice::MaxVolume);
    CHECK(bob.voice.participants().at(0).volume == rtype::voice::MaxVolume);

    bob.voice.removeParticipant(0);
    CHECK_FALSE(bob.playback.streams[0]);
    CHECK(bob.voice.participants().empty());
    CHECK(talkFor(10) == 0);
}

TEST_CASE("A game loop that stalls sends the newest 60 ms of the microphone, not the voice of long ago",
          "[voice]")
{
    Player alice(0, 1);
    alice.talk();
    Voice voice;
    for (int i = 0; i < 25; ++i) {
        alice.microphone.say(voice.next());
    }
    alice.voice.poll(Start);
    CHECK(alice.transport.sent.size() == GameVoice::MaxCaptureBacklog / FrameSamples);
}

TEST_CASE("Without a microphone the player listens, and leaving closes every device", "[voice]")
{
    FakeTransport transport;
    FakeMicrophone microphone;
    microphone.present = false;
    FakePlayback playback;
    GameVoice voice(transport, microphone, playback);
    // Known before the slot of this client: the player itself is not one of the others
    voice.addParticipant(0);
    voice.addParticipant(2);
    CHECK_FALSE(playback.streams[2]);
    voice.join({.self = 0});
    CHECK(playback.streams[2]);
    CHECK_FALSE(playback.streams[0]);
    REQUIRE(voice.participants().size() == 1);
    CHECK(voice.participants()[0].slot == 2);
    CHECK(voice.participants()[0].backend == Backend::None);

    voice.setSelfMute(false);
    CHECK(microphone.opens == 1);
    CHECK_FALSE(voice.microphoneOpen());

    voice.leave();
    CHECK_FALSE(voice.joined());
    CHECK_FALSE(playback.streams[2]);
    CHECK(voice.participants().empty());
}
