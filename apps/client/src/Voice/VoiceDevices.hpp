#pragma once

#include "rtype/voice/Devices.hpp"
#include "rtype/voice/Talk.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace rtype::client
{

/// How the player hears another player, chosen in the pause menu of a game: kept for the next games.
struct PlayerVoice {
    /// From 0 to 1.
    float volume = voice::DefaultVolume;
    bool muted = false;

    friend bool operator==(const PlayerVoice &, const PlayerVoice &) = default;
};

/// What the choices for another player are kept under: its account on the server of the game, or its name
/// there for a guest.
/// @param server The address of the server, as the player wrote it ("host:port").
/// @param accountId The id of its account, 0 for a guest (`PLAYER_JOINED`).
[[nodiscard]] std::string playerVoiceKey(std::string_view server, std::uint64_t accountId,
                                         std::string_view name);

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
    /// How it hears each other player it changed, by playerVoiceKey(). A player not there is heard at full
    /// volume.
    std::map<std::string, PlayerVoice, std::less<>> players{};

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
