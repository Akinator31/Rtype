#pragma once

#include "rtype/voice/Devices.hpp"
#include "rtype/voice/Talk.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <string>

namespace rtype::client
{

/// How the player wants to talk and hear, from the Voice page (Settings/VoiceSettings.hpp).
struct VoicePreferences {
    voice::TalkMode mode = voice::TalkMode::PushToTalk;
    /// The level over which the voice activity hears the player, in dBFS.
    float threshold = voice::DefaultThreshold;
    /// The name of the microphone, empty for the default of the system.
    std::string inputDevice;
    /// The volume of the microphone, and of the voices of the others, from 0 to 1.
    float inputVolume = voice::DefaultVolume;
    float voiceVolume = voice::DefaultVolume;

    friend bool operator==(const VoicePreferences &, const VoicePreferences &) = default;
};

/// What the voice of a game online needs (docs/architecture/voice.md): the devices, which outlive the
/// sessions, and how the player talks. The Voice page writes the preferences, the VoiceKeysSystem the keys,
/// each RemoteSession reads them every update.
struct VoiceDevices {
    voice::Capture &capture;
    voice::Playback &playback;
    VoicePreferences preferences{};
    /// Muted with its key: nobody hears the player, whatever the mode. Until the client closes.
    bool muted = false;
    /// Deafened with its key: the player hears no voice, and is muted too. Until the client closes.
    bool deafened = false;
    /// The push-to-talk key is held, in a game.
    bool pushToTalk = false;
    /// The microphone of the game played gives only silence while the player talks
    /// (voice::GameVoice::microphoneSilent()): written by the session, read by the VoiceKeysSystem, which
    /// warns the player.
    bool microphoneSilent = false;
};

} // namespace rtype::client
