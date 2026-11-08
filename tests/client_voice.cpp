// Tests of the voice of the client: its preferences kept in the settings, and how they and the keys drive the
// voice of the game.

#include "FakeVoiceDevices.hpp"
#include "Game/RemoteSession.hpp"
#include "Settings/VoiceSettings.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/voice/GameVoice.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

using rtype::client::applyVoice;
using rtype::client::readVoiceSettings;
using rtype::client::VoiceDevices;
using rtype::client::VoicePreferences;
using rtype::voice::TalkMode;

namespace
{

/// A player alone in a game, with the devices of its voice.
struct Player {
    testing::FakeTransport transport;
    testing::FakeMicrophone microphone;
    testing::FakePlayback playback;
    rtype::voice::GameVoice voice{transport, microphone, playback};
    VoiceDevices devices{.capture = microphone, .playback = playback};
    double phase = 0.0;

    Player()
    {
        voice.addParticipant(1);
        voice.setBackend(1, rtype::voice::Backend::Game);
        voice.join({.self = 0});
    }

    /// Applies the devices, then says this many frames of a loud tone.
    /// @return The frames sent.
    std::size_t talk(bool pushToTalk, int frames = 5)
    {
        applyVoice(voice, devices, pushToTalk);
        transport.sent.clear();
        for (int i = 0; i < frames; ++i) {
            std::array<std::int16_t, rtype::voice::FrameSamples> frame{};
            for (std::int16_t &sample : frame) {
                phase += 2.0 * std::numbers::pi * 180.0 / rtype::voice::SampleRate;
                sample = static_cast<std::int16_t>(std::sin(phase) * 8000.0 + std::sin(3.0 * phase) * 3000.0);
            }
            microphone.say(frame);
            voice.poll();
        }
        return transport.sent.size();
    }
};

} // namespace

TEST_CASE("A first run talks in push-to-talk, at full volume, with the default microphone",
          "[settings][voice]")
{
    const rtype::engine::Settings settings;
    const VoicePreferences preferences = readVoiceSettings(settings);

    CHECK(preferences == VoicePreferences{});
    CHECK(preferences.mode == TalkMode::PushToTalk);
    CHECK(preferences.threshold == rtype::voice::DefaultThreshold);
    CHECK(preferences.inputDevice.empty());
    CHECK(preferences.inputVolume == 1.0f);
    CHECK(preferences.voiceVolume == 1.0f);
}

TEST_CASE("The preferences of the voice are kept in the settings and found again", "[settings][voice]")
{
    const VoicePreferences chosen{.mode = TalkMode::VoiceActivity,
                                  .threshold = -52.5f,
                                  .inputDevice = "USB Headset Mono",
                                  .inputVolume = 0.75f,
                                  .voiceVolume = 0.4f};
    rtype::engine::Settings settings;
    rtype::client::keepVoiceSettings(settings, chosen);

    CHECK(settings.getString("voice.mode") == "voice_activity");
    CHECK(settings.getString("voice.input_device") == "USB Headset Mono");
    CHECK(readVoiceSettings(settings) == chosen);
}

TEST_CASE("The choices for the other players are kept by account, or by name for a guest",
          "[settings][voice]")
{
    using rtype::client::PlayerVoice;
    using rtype::client::playerVoiceKey;
    CHECK(playerVoiceKey("rtype.example:4242", 7, "Nova") == "rtype.example:4242/7");
    CHECK(playerVoiceKey("rtype.example:4242", 0, "~Nova") == "rtype.example:4242/guest/~Nova");
    // The same account on another server is another player
    CHECK(playerVoiceKey("10.0.0.7:4242", 7, "Nova") != playerVoiceKey("rtype.example:4242", 7, "Nova"));

    VoicePreferences chosen;
    chosen.players[playerVoiceKey("10.0.0.7:4242", 7, "Nova")] = {.volume = 0.35f, .muted = false};
    chosen.players[playerVoiceKey("10.0.0.7:4242", 0, "~Orion")] = {.volume = 1.0f, .muted = true};
    // Heard as by default: not kept
    chosen.players[playerVoiceKey("10.0.0.7:4242", 9, "Vortex")] = {};
    rtype::engine::Settings settings;
    settings.setFloat("voice.player.old:4242/3.volume", 0.2f);
    rtype::client::keepVoiceSettings(settings, chosen);

    CHECK(settings.getFloat("voice.player.10.0.0.7:4242/7.volume") == 0.35f);
    CHECK(settings.getBool("voice.player.10.0.0.7:4242/guest/~Orion.muted") == true);
    CHECK_FALSE(settings.hasKey("voice.player.10.0.0.7:4242/9.volume"));
    // A player forgotten since is not kept either
    CHECK_FALSE(settings.hasKey("voice.player.old:4242/3.volume"));
    const VoicePreferences read = readVoiceSettings(settings);
    CHECK(read.players.size() == 2);
    CHECK(read.players.at(playerVoiceKey("10.0.0.7:4242", 7, "Nova")) == PlayerVoice{0.35f, false});
    CHECK(read.players.at(playerVoiceKey("10.0.0.7:4242", 0, "~Orion")) == PlayerVoice{1.0f, true});

    // A volume out of range is clamped
    settings.setFloat("voice.player.10.0.0.7:4242/7.volume", 4.0f);
    CHECK(readVoiceSettings(settings).players.at(playerVoiceKey("10.0.0.7:4242", 7, "Nova")).volume == 1.0f);
}

TEST_CASE("The voice of the game tells what the player does with it", "[voice]")
{
    Player player;
    rtype::voice::GameVoice &voice = player.voice;
    voice.setSelfMute(false);
    CHECK(voice.microphoneOpen());
    CHECK_FALSE(voice.microphoneMissing());

    // Unplugged: it cannot open again, and listens only
    voice.setSelfMute(true);
    player.microphone.present = false;
    voice.setSelfMute(false);
    CHECK(voice.microphoneMissing());

    // What another player tells is given to the HUD
    voice.setStatus(1, {.muted = true});
    CHECK(voice.participants().at(0).status == rtype::voice::Status{.muted = true});
}

TEST_CASE("Wrong preferences of the voice leave the defaults, numbers out of range are clamped",
          "[settings][voice]")
{
    rtype::engine::Settings settings;
    settings.setString("voice.mode", "shout");
    settings.setString("voice.threshold", "loud");
    settings.setFloat("voice.input_volume", 3.0f);
    settings.setFloat("voice.volume", -1.0f);

    VoicePreferences preferences = readVoiceSettings(settings);
    CHECK(preferences.mode == TalkMode::PushToTalk);
    CHECK(preferences.threshold == rtype::voice::DefaultThreshold);
    CHECK(preferences.inputVolume == 1.0f);
    CHECK(preferences.voiceVolume == 0.0f);

    settings.setFloat("voice.threshold", -200.0f);
    CHECK(readVoiceSettings(settings).threshold == rtype::voice::MinThreshold);
    settings.setFloat("voice.threshold", 5.0f);
    CHECK(readVoiceSettings(settings).threshold == rtype::voice::MaxThreshold);
}

TEST_CASE("In push-to-talk the player is heard while the key is held", "[voice]")
{
    Player player;
    CHECK(player.talk(false) == 0);
    // The microphone is open all the same: the key held is heard at once
    CHECK(player.microphone.isOpen);
    CHECK(player.talk(true) == 5);
}

TEST_CASE("With the voice activity the player is heard without the key; off, never", "[voice]")
{
    Player player;
    player.devices.preferences.mode = TalkMode::VoiceActivity;
    CHECK(player.talk(false) == 5);
    // Over a threshold louder than the voice: not heard
    player.devices.preferences.threshold = rtype::voice::MaxThreshold;
    // Only the last frame of the spurt before, to stop the others at once
    CHECK(player.talk(false) == 1);
    CHECK(player.transport.sent.back().last);
    CHECK(player.talk(false) == 0);

    player.devices.preferences.mode = TalkMode::Off;
    CHECK(player.talk(true) == 0);
    CHECK_FALSE(player.microphone.isOpen);
}

TEST_CASE("Muted or deafened by its keys, the player is not heard; deafened, it hears nothing", "[voice]")
{
    Player player;
    player.devices.muted = true;
    CHECK(player.talk(true) == 0);
    CHECK_FALSE(player.microphone.isOpen);
    CHECK_FALSE(player.voice.deafened());

    player.devices.muted = false;
    player.devices.deafened = true;
    CHECK(player.talk(true) == 0);
    CHECK(player.voice.deafened());
    CHECK(player.playback.volume[1] == 0.0f);

    player.devices.deafened = false;
    player.devices.preferences.voiceVolume = 0.5f;
    CHECK(player.talk(true) == 5);
    CHECK(player.playback.volume[1] == 0.5f);
}
