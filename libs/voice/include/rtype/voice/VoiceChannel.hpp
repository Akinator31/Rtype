#pragma once

#include "rtype/voice/Backend.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

/// The voice chat of an online game (docs/architecture/voice.md, ADR 0032): the client knows this interface
/// only, whatever carries the voice of each player, the game or Discord. Its implementations come with the
/// steps of the voice; the tests use a fake (tests/helpers/FakeVoiceChannel.hpp).
namespace rtype::voice
{

/// A player of the game, by the slot the game protocol gives it (`PLAYER_JOINED`): 0 to 3.
using Slot = std::uint8_t;

/// The most players of a game, so of a voice channel.
inline constexpr std::size_t MaxParticipants = 4;

/// The volume of a player when nobody changed it, and the loudest one allowed.
inline constexpr float DefaultVolume = 1.0f;
inline constexpr float MaxVolume = 2.0f;

/// Where a player is, for the spatial voice: the position of its ship, in the units of the game. A 3D vector
/// from the start, for the 3D and VR of later; z is 0 in the 2D game.
struct Position {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    friend bool operator==(const Position &, const Position &) = default;
};

/// Where the player of this client is, given when the game is joined.
struct Join {
    /// The slot of this client in the game (`ACCEPTED`).
    Slot self = 0;

    friend bool operator==(const Join &, const Join &) = default;
};

/// Another player of the game, as the channel knows it.
struct Participant {
    Slot slot = 0;
    /// What carries its voice (`VOICE_ROSTER`).
    Backend backend = Backend::None;
    /// It talks now: lit in the HUD.
    bool speaking = false;
    /// Muted by itself.
    bool selfMuted = false;
    /// Muted by the player of this client, for this client only.
    bool mutedByMe = false;
    /// Its volume for this client, from 0 to MaxVolume.
    float volume = DefaultVolume;

    friend bool operator==(const Participant &, const Participant &) = default;
};

/// The voice of one online game, for the player of this client.
///
/// @code
/// voice.join({.self = accepted.slot});
/// voice.addParticipant(joined.slot);                // PLAYER_JOINED
/// voice.setBackend(slot, roster.backend(slot));     // VOICE_ROSTER
/// // In the game loop, every frame
/// voice.setTalking(actions.isActive(Action::PushToTalk));
/// voice.poll();
/// for (const rtype::voice::Participant &player : voice.participants()) {
///     hud.setSpeaking(player.slot, player.speaking);
/// }
/// voice.leave();                                     // LEFT_GAME, or the session lost
/// @endcode
///
/// Every call is made on the thread of the game loop. A device that cannot be opened is not an error: the
/// player hears nothing, or is heard by nobody, and the game goes on.
class VoiceChannel
{
public:
    virtual ~VoiceChannel() = default;

    /// Joins the voice of the game this client just joined. Another join() leaves the one before.
    virtual void join(const Join &join) = 0;
    /// Leaves it: the microphone and the voices of the others are closed. Nothing if not joined.
    virtual void leave() = 0;

    /// A player came in the game, on Backend::None until the roster says otherwise. The slot of this client
    /// is ignored.
    virtual void addParticipant(Slot slot) = 0;
    /// A player left the game: its voice stops and its settings for this game are forgotten.
    virtual void removeParticipant(Slot slot) = 0;
    /// What carries the voice of a player, from the roster of the game.
    virtual void setBackend(Slot slot, Backend backend) = 0;

    /// Mutes the player of this client: nobody hears it, whatever it does.
    virtual void setSelfMute(bool muted) = 0;
    /// Whether the player of this client talks now: push-to-talk held, or a voice detected. Nothing goes out
    /// while it is false.
    virtual void setTalking(bool talking) = 0;

    /// The volume of a player for this client, clamped to [0, MaxVolume].
    virtual void setParticipantVolume(Slot slot, float volume) = 0;
    /// Mutes a player for this client only.
    virtual void setParticipantMute(Slot slot, bool muted) = 0;
    /// Where a player is, for the spatial voice.
    virtual void setParticipantPosition(Slot slot, Position position) = 0;

    /// The other players of the game, by slot.
    [[nodiscard]] virtual std::vector<Participant> participants() const = 0;

    /// Runs what came since the last call: voices received, players who started or stopped talking. Once per
    /// frame, in the game loop.
    virtual void poll() = 0;
};

} // namespace rtype::voice
