#pragma once

#include "rtype/discord/VoiceCall.hpp"
#include "rtype/voice/Devices.hpp"
#include "rtype/voice/Processing.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The voice of a game carried by Discord (docs/architecture/voice.md#the-voice-of-discord, ADR 0035).
namespace rtype::discord
{

/// The secret of a Discord lobby from the 32 bytes of GAME_JOINED: their hexadecimal, 64 characters. Secret:
/// never logged.
[[nodiscard]] std::string lobbySecret(std::span<const std::byte, 32> secret);

/// The voice of a game carried by the Discord lobby of the game: the player of this client is on
/// Backend::Discord once its call is connected.
///
/// - **join()** joins the lobby of the secret with the id the master gave the player as its `player`
///   metadata, then starts its call. The other members are matched to the slots of the game by theirs: the
///   id of each player comes with PLAYER_JOINED (setPlayerId()). A member matched to no player, one kicked
///   who kept the secret, is not heard.
/// - **In**: Discord gives the voice of each member on its audio thread; it goes to a ring buffer of its
///   slot, without a lock, at 48 kHz mono (resampled and mixed down if needed), and poll() writes it to the
///   stream of the slot of the Playback, as the voice of the game: the same mixer, the same volumes. Discord
///   plays none of it. The streams are opened once the call is connected, and closed when it ends.
/// - **Out**: Discord captures and sends the microphone itself. Muted, deafened, the talk mode and the
///   push-to-talk key, the volume of the microphone and its processing are given to the call. What it
///   captures is also given, before its processing, to capture(): in a mixed game, the voice of the game
///   sends it to the players on the game (docs/architecture/voice.md#mixed-games).
///
/// @code
/// discord::DiscordVoice voice(call, playback, discord::lobbySecret(*joined.voiceSecret), playerId);
/// voice.setPlayerId(slot, playerJoined.playerId);    // PLAYER_JOINED, with addParticipant()
/// voice.join({.self = accepted.slot});
/// voice.poll();    // each frame of the game loop
/// if (voice.failed()) { /* the voice of the game takes over */ }
/// @endcode
///
/// Every call is made on the thread of the game loop. The call and the playback must outlive it.
class DiscordVoice final : public voice::VoiceChannel
{
public:
    /// Samples of each member kept for the next poll() at most: 500 ms. Beyond, Discord's are dropped.
    static constexpr std::size_t InboxSamples = 24000;
    /// Samples of the microphone kept for the voice of the game at most: 500 ms.
    static constexpr std::size_t CaptureSamples = 24000;

    /// What the audio thread of Discord writes, shared with the call: it may run a little after the voice is
    /// gone.
    struct Inbox;
    /// The microphone of Discord, as a voice::Capture.
    class Capture;

private:
    struct Remote {
        voice::Participant player;
        /// The id the master gave it, from PLAYER_JOINED.
        std::optional<std::uint32_t> playerId;
    };

    VoiceCall &_call;
    voice::Playback &_playback;
    std::string _secret;
    std::uint32_t _playerId;
    std::shared_ptr<Inbox> _inbox;
    std::unique_ptr<Capture> _capture;
    std::optional<voice::Join> _joined;
    std::array<std::optional<Remote>, voice::MaxParticipants> _remotes;
    bool _streamsOpen = false;
    bool _selfMuted = true;
    bool _deafened = false;
    bool _talking = false;
    std::optional<float> _threshold;
    float _inputVolume = voice::DefaultVolume;
    float _outputVolume = voice::DefaultVolume;
    voice::ProcessingSettings _processing{};

    [[nodiscard]] Remote *remote(voice::Slot slot);
    /// Whether a player is heard by this client: not muted by it, nor by an administrator, nor deafened.
    [[nodiscard]] bool heard(const Remote &remote) const;
    void applyVolume(const Remote &remote);
    /// Gives the call how the player talks now.
    void applyTalk();
    void openStreams();
    void closeStreams();
    /// Matches the members of the call to the slots, by their `player`.
    void match();
    void play();

public:
    /// @param secret The secret of the lobby (lobbySecret()).
    /// @param playerId The id the master gave the player (WELCOME): its `player` in the lobby.
    DiscordVoice(VoiceCall &call, voice::Playback &playback, std::string secret, std::uint32_t playerId);
    /// Ends the call and leaves the lobby.
    ~DiscordVoice() override;

    DiscordVoice(const DiscordVoice &) = delete;
    DiscordVoice &operator=(const DiscordVoice &) = delete;

    void join(const voice::Join &join) override;
    void leave() override;
    void addParticipant(voice::Slot slot) override;
    void removeParticipant(voice::Slot slot) override;
    void setBackend(voice::Slot slot, voice::Backend backend) override;
    void setStatus(voice::Slot slot, voice::Status status) override;
    void setSelfMute(bool muted) override;
    void setTalking(bool talking) override;
    void setParticipantVolume(voice::Slot slot, float volume) override;
    void setParticipantMute(voice::Slot slot, bool muted) override;
    void setParticipantPosition(voice::Slot slot, voice::Position position) override;
    [[nodiscard]] std::vector<voice::Participant> participants() const override;
    void poll() override;

    /// The id the master gave a player (PLAYER_JOINED), which its member of the lobby has as its `player`.
    void setPlayerId(voice::Slot slot, std::uint32_t playerId);
    /// Talks with the voice activity of Discord over this level in dBFS, or with the push-to-talk of
    /// setTalking() without it.
    void setVoiceActivity(std::optional<float> threshold);
    /// The volume of the microphone, from 0 to MaxVolume: Discord takes up to 1.
    void setInputVolume(float volume);
    /// The noise suppression and the gain control, those of Discord.
    void setProcessing(const voice::ProcessingSettings &settings);
    /// The volume of every voice, from 0 to MaxVolume, times the volume of each player.
    void setOutputVolume(float volume);
    /// Deafens the player in the call, which mutes it too, and plays no voice.
    void setDeafened(bool deafened);

    /// In the voice of a game: join() was called.
    [[nodiscard]] bool joined() const { return _joined.has_value(); }
    /// The call is connected: the voice of the player goes through Discord.
    [[nodiscard]] bool connected() const;
    /// The lobby or the call could not be joined, or the call dropped: the voice of the game takes over.
    [[nodiscard]] bool failed() const;
    /// The voice of the player goes out now, as Discord says.
    [[nodiscard]] bool transmitting() const;
    [[nodiscard]] bool selfMuted() const { return _selfMuted; }
    [[nodiscard]] bool deafened() const { return _deafened; }
    /// Discord opens the microphone itself and says nothing of it: always false.
    [[nodiscard]] bool microphoneMissing() const { return false; }
    /// Discord says nothing of a silent microphone: always false.
    [[nodiscard]] bool microphoneSilent() const { return false; }

    /// The microphone as Discord captures it, before its processing, at 48 kHz mono (resampled and mixed
    /// down if needed): what the voice of the game sends in a mixed game (voice::GameVoice::setCapture()).
    /// Open, it takes what Discord captures from then on; it never fails, Discord having the microphone.
    [[nodiscard]] voice::Capture &capture();
    /// The samples Discord captured since the start, read or not: whether it gives its microphone at all.
    [[nodiscard]] std::uint64_t capturedSamples() const;
};

} // namespace rtype::discord
