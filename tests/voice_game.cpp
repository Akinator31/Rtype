#include "helpers/FakeVoiceDevices.hpp"
#include "rtype/voice/GameVoice.hpp"
#include "rtype/voice/SharedPlayback.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <random>
#include <span>
#include <vector>

using namespace std::chrono_literals;
using rtype::voice::Backend;
using rtype::voice::FrameSamples;
using rtype::voice::GameVoice;
using rtype::voice::SampleRate;
using testing::FakeLoopback;
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
        // The samples as they are said: the processing has its tests (voice_processing.cpp)
        voice.setProcessing({.noiseSuppression = false, .gainControl = false});
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

TEST_CASE("With the voice activity, only the frames of the voice go, and those just after", "[voice]")
{
    Player alice(0, 1);
    alice.talk();
    alice.voice.setVoiceActivity(-40.0f);
    Voice voice;
    // The breath of the room, under the threshold
    const auto quiet = [&voice] {
        Frame frame = voice.next();
        for (std::int16_t &sample : frame) {
            sample = static_cast<std::int16_t>(sample / 100);
        }
        return frame;
    };
    for (int i = 0; i < 20; ++i) {
        alice.microphone.say(quiet());
        alice.voice.poll(Start);
    }
    CHECK(alice.transport.sent.empty());
    CHECK(alice.voice.inputLevel() < -40.0f);

    for (int i = 0; i < 5; ++i) {
        alice.microphone.say(voice.next());
        alice.voice.poll(Start);
    }
    CHECK(alice.transport.sent.size() == 5);
    CHECK(alice.voice.inputLevel() > -40.0f);

    // The voice stops: the hold, then the last frame of the spurt, then nothing
    for (int i = 0; i < 30; ++i) {
        alice.microphone.say(quiet());
        alice.voice.poll(Start);
    }
    REQUIRE_FALSE(alice.transport.sent.empty());
    CHECK(alice.transport.sent.back().last);
    CHECK(alice.transport.sent.size() <= 5 + rtype::voice::VoiceActivity::Hold + 1);
    const std::size_t sent = alice.transport.sent.size();
    for (int i = 0; i < 10; ++i) {
        alice.microphone.say(quiet());
        alice.voice.poll(Start);
    }
    CHECK(alice.transport.sent.size() == sent);
}

TEST_CASE("The volume of the microphone is applied before the level and the encoder", "[voice]")
{
    Player alice(0, 1);
    alice.talk();
    Voice voice;
    alice.microphone.say(voice.next());
    alice.voice.poll(Start);
    const float full = alice.voice.inputLevel();

    alice.voice.setInputVolume(0.5f);
    alice.microphone.say(voice.next());
    alice.voice.poll(Start);
    CHECK(std::abs(alice.voice.inputLevel() - (full - 6.02f)) < 0.5f);

    // At 0, the others hear silence: Opus stops sending it
    alice.voice.setInputVolume(0.0f);
    alice.transport.sent.clear();
    for (int i = 0; i < 50; ++i) {
        alice.microphone.say(voice.next());
        alice.voice.poll(Start);
    }
    CHECK(alice.voice.inputLevel() == rtype::voice::SilenceFloor);
    CHECK(alice.transport.sent.size() <= 10);
}

TEST_CASE("Deafened, the player hears no voice; the voice volume scales every player", "[voice]")
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
        for (int i = 0; i < 10; ++i) {
            bob.voice.poll(now);
            now += 20ms;
        }
        return bob.playback.played[0].size();
    };

    bob.voice.setDeafened(true);
    CHECK(bob.voice.deafened());
    CHECK(bob.playback.volume[0] == 0.0f);
    CHECK(talkFor(10) == 0);
    CHECK_FALSE(bob.voice.participants().at(0).speaking);

    bob.voice.setDeafened(false);
    CHECK(bob.playback.volume[0] == rtype::voice::DefaultVolume);
    CHECK(talkFor(10) > 0);

    bob.voice.setOutputVolume(0.5f);
    CHECK(bob.playback.volume[0] == 0.5f);
    bob.voice.setParticipantVolume(0, rtype::voice::MaxVolume);
    CHECK(bob.playback.volume[0] == rtype::voice::DefaultVolume);
    bob.voice.setOutputVolume(rtype::voice::MaxVolume);
    CHECK(bob.playback.volume[0] == rtype::voice::MaxVolume);
}

TEST_CASE("A microphone that gives only silence for 10 s while the player talks is said silent", "[voice]")
{
    Player alice(0, 1);
    alice.talk();
    const Frame silence{};
    for (std::size_t i = 0; i + 1 < GameVoice::SilentFrames; ++i) {
        alice.microphone.say(silence);
        alice.voice.poll(Start);
    }
    CHECK_FALSE(alice.voice.microphoneSilent());
    alice.microphone.say(silence);
    alice.voice.poll(Start);
    CHECK(alice.voice.microphoneSilent());

    // A sound: it works
    Voice voice;
    alice.microphone.say(voice.next());
    alice.voice.poll(Start);
    CHECK_FALSE(alice.voice.microphoneSilent());

    // Not talking, the silence of the microphone says nothing
    alice.voice.setTalking(false);
    for (std::size_t i = 0; i < 2 * GameVoice::SilentFrames; ++i) {
        alice.microphone.say(silence);
        alice.voice.poll(Start);
    }
    CHECK_FALSE(alice.voice.microphoneSilent());
}

TEST_CASE("The echo of what the speakers play is not sent, the loopback open with the microphone", "[voice]")
{
    // What the speakers play, and the microphone hearing it 60 ms later, at half its level: about -24 dBFS,
    // over the threshold of the voice activity
    constexpr std::size_t frames = 8 * rtype::voice::FramesPerSecond;
    std::mt19937 random(11);
    std::normal_distribution<double> normal(0.0, 4000.0);
    std::vector<std::int16_t> played(frames * FrameSamples);
    for (std::int16_t &sample : played) {
        sample = static_cast<std::int16_t>(std::clamp(normal(random), -32768.0, 32767.0));
    }
    constexpr std::size_t delay = 60 * static_cast<std::size_t>(SampleRate) / 1000;
    std::vector<std::int16_t> heard(played.size(), 0);
    for (std::size_t i = delay; i < heard.size(); ++i) {
        heard[i] = static_cast<std::int16_t>(played[i - delay] / 2);
    }

    const auto sentAtTheEnd = [&](bool withLoopback) {
        Player alice(0, 1);
        FakeLoopback loopback;
        GameVoice voice(alice.transport, alice.microphone, alice.playback,
                        withLoopback ? &loopback : nullptr);
        voice.setProcessing({.noiseSuppression = false, .gainControl = false});
        voice.join({.self = 0});
        CHECK_FALSE(loopback.isOpen);
        voice.setSelfMute(false);
        voice.setTalking(true);
        voice.setVoiceActivity(-40.0f);
        CHECK(loopback.isOpen == withLoopback);
        std::size_t sent = 0;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            loopback.play(std::span(played).subspan(frame * FrameSamples, FrameSamples));
            alice.microphone.say(std::span(heard).subspan(frame * FrameSamples, FrameSamples));
            const std::size_t before = alice.transport.sent.size();
            voice.poll(Start);
            // The last 2 s, the room learnt
            if (frame >= frames - (2 * rtype::voice::FramesPerSecond)) {
                sent += alice.transport.sent.size() - before;
            }
        }
        voice.setSelfMute(true);
        CHECK_FALSE(loopback.isOpen);
        return sent;
    };
    CHECK(sentAtTheEnd(false) == 2 * rtype::voice::FramesPerSecond);
    CHECK(sentAtTheEnd(true) == 0);
}

TEST_CASE("The voice of a player can come from another capture, the one before closed", "[voice]")
{
    Player alice(0, 1);
    Voice voice;
    alice.talk();
    for (int i = 0; i < 3; ++i) {
        alice.microphone.say(voice.next());
        alice.voice.poll(Start);
    }
    REQUIRE(alice.transport.sent.size() == 3);

    // Another capture, as the microphone of Discord in a mixed game: the microphone closed, it opened
    FakeMicrophone discord;
    alice.voice.setCapture(discord);
    CHECK_FALSE(alice.microphone.isOpen);
    CHECK(discord.isOpen);
    CHECK(alice.voice.microphoneOpen());
    alice.microphone.say(voice.next());
    discord.say(voice.next());
    discord.say(voice.next());
    alice.voice.poll(Start);
    CHECK(alice.transport.sent.size() == 5);

    // Muted, then given back the microphone: nothing opened until it talks again
    alice.voice.setSelfMute(true);
    CHECK_FALSE(discord.isOpen);
    alice.voice.setCapture(alice.microphone);
    CHECK_FALSE(alice.microphone.isOpen);
    alice.voice.setSelfMute(false);
    CHECK(alice.microphone.isOpen);
    CHECK_FALSE(discord.isOpen);

    // One that cannot open: the player listens only
    FakeMicrophone missing;
    missing.present = false;
    alice.voice.setCapture(missing);
    CHECK_FALSE(alice.microphone.isOpen);
    CHECK(alice.voice.microphoneMissing());
}

TEST_CASE("Two voices share one playback, each slot heard from the one source the roster gives it", "[voice]")
{
    FakePlayback streams;
    rtype::voice::SharedPlayback playbacks(streams);
    rtype::voice::Playback &game = playbacks.game();
    rtype::voice::Playback &discord = playbacks.discord();
    const std::vector<std::int16_t> samples(FrameSamples, 1000);

    // From the game by default: what Discord writes is dropped
    CHECK(playbacks.source(1) == Backend::Game);
    game.open(1);
    discord.open(1);
    CHECK(streams.streams[1]);
    CHECK(game.write(1, samples));
    CHECK(discord.write(1, samples));
    CHECK(streams.played[1].size() == FrameSamples);

    // Each source gives its volume; the stream has the one of its source
    game.setVolume(1, 0.5f);
    discord.setVolume(1, 1.5f);
    CHECK(streams.volume[1] == 0.5f);
    playbacks.setSource(1, Backend::Discord);
    CHECK(streams.volume[1] == 1.5f);
    CHECK(game.write(1, samples));
    CHECK(discord.write(1, samples));
    CHECK(streams.played[1].size() == 2 * FrameSamples);
    // None is the game: a player without a voice is heard from nothing anyway
    playbacks.setSource(1, Backend::None);
    CHECK(playbacks.source(1) == Backend::Game);
    CHECK(streams.volume[1] == 0.5f);

    // Open while either has it open
    discord.close(1);
    CHECK(streams.streams[1]);
    game.close(1);
    CHECK_FALSE(streams.streams[1]);
    discord.open(1);
    CHECK(streams.streams[1]);
    game.close(1);
    CHECK(streams.streams[1]);

    // A slot out of range is nothing
    CHECK(discord.write(9, samples));
    discord.open(9);
}
