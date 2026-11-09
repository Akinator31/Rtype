#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

/// The Discord lobby of a game and its call (docs/architecture/voice.md#the-voice-of-discord): the players
/// of a game who have Discord join the lobby of its secret (GAME_JOINED) and talk in its call. The client
/// and its tests know only this interface; SocialSdkVoiceCall (rtype/discord/SocialSdkVoiceCall.hpp) is its
/// implementation on the Social SDK, and DiscordVoice (rtype/discord/DiscordVoice.hpp) the voice of the game
/// built on it.
namespace rtype::discord
{

/// Where the call is.
enum class CallStatus {
    /// Never started, or ended.
    Idle,
    /// start() was called: the lobby is being joined, then the call connected.
    Joining,
    /// The voice goes both ways.
    Connected,
    /// The lobby or the call could not be joined, or the call dropped once connected: the voice of the game
    /// takes over. end() makes it Idle again.
    Failed,
};

/// Another member of the lobby, connected to its call.
struct CallMember {
    /// Its id on Discord.
    std::uint64_t userId = 0;
    /// What it joined the lobby with: `player`, the id the master gave it (WELCOME, PLAYER_JOINED).
    std::map<std::string, std::string> metadata;
    /// Discord hears it talk now.
    bool speaking = false;

    friend bool operator==(const CallMember &, const CallMember &) = default;
};

/// How the call knows the player talks.
enum class CallTalkMode {
    /// While setPushToTalk(true).
    PushToTalk,
    /// When its microphone is over the threshold.
    VoiceActivity,
};

/// The Discord lobby of a game, and its call.
///
/// @code
/// call.start(secret, {{"player", "42"}}, [inbox](std::uint64_t user, std::span<const std::int16_t> samples,
///                                                std::int32_t rate, std::size_t channels) {
///     // On a thread of Discord: the voice of a member, for the mixer of the game
/// });
/// // In the game loop
/// if (call.status() == rtype::discord::CallStatus::Connected) {
///     for (const rtype::discord::CallMember &member : call.members()) { /* match it to a slot */ }
/// }
/// call.end();    // LEFT_GAME, or the session lost
/// @endcode
///
/// Every call is made on the thread that polls Discord, which runs its answers: in the client, the game loop.
/// Only the voices received come on another thread, the one of the audio of Discord. Without Discord
/// connected, start() fails: the player is on the voice of the game.
class VoiceCall
{
public:
    /// The voice of a member, `samples` interleaved, `channels` per frame, at `sampleRate`. Discord does not
    /// play it: the game does, through its own mixer, with its volumes. Called on the audio thread of
    /// Discord, never waiting: it must not lock. It may run a little after end(): what it uses must outlive
    /// the call, by being shared with it.
    using Received = std::function<void(std::uint64_t userId, std::span<const std::int16_t> samples,
                                        std::int32_t sampleRate, std::size_t channels)>;

    virtual ~VoiceCall() = default;

    /// Joins the lobby of `secret` (never logged) with `metadata`, then starts its call, the voices received
    /// given to `received`. Another start() ends the call before.
    virtual void start(const std::string &secret, const std::map<std::string, std::string> &metadata,
                       Received received) = 0;
    /// Ends the call and leaves the lobby. Nothing if Idle.
    virtual void end() = 0;
    [[nodiscard]] virtual CallStatus status() const = 0;
    /// The other members connected to the call, by user id.
    [[nodiscard]] virtual std::vector<CallMember> members() const = 0;
    /// Discord hears the player talk now: its voice goes out.
    [[nodiscard]] virtual bool speaking() const = 0;

    /// Mutes the microphone of the player in the call.
    virtual void setSelfMute(bool muted) = 0;
    /// Deafens the player: Discord sends it no voice, and mutes it.
    virtual void setSelfDeaf(bool deafened) = 0;
    /// How the player talks; `threshold` is the level of the voice activity, in dBFS.
    virtual void setTalkMode(CallTalkMode mode, float threshold) = 0;
    /// The push-to-talk key is held.
    virtual void setPushToTalk(bool active) = 0;
    /// The volume of the microphone, from 0 to 1.
    virtual void setInputVolume(float volume) = 0;
    /// The noise suppression and the gain control of Discord on the microphone.
    virtual void setProcessing(bool noiseSuppression, bool gainControl) = 0;
};

} // namespace rtype::discord
