#pragma once

#include "rtype/voice/Codec.hpp"
#include "rtype/voice/Devices.hpp"
#include "rtype/voice/JitterBuffer.hpp"
#include "rtype/voice/Processing.hpp"
#include "rtype/voice/Talk.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

/// The voice carried by the game (docs/architecture/voice.md#the-voice-of-the-game, ADR 0032).
namespace rtype::voice
{

/// What the voice of the game goes through: the session with the game, which the client gives
/// (RemoteSession) and the tests fake.
class VoiceTransport
{
public:
    /// A frame of the player, to the game (`VOICE`).
    struct Outgoing {
        std::uint16_t sequence = 0;
        bool last = false;
        std::span<const std::uint8_t> bytes;
    };
    /// A frame of another player, from the game (`VOICE_FRAME`).
    struct Incoming {
        Slot slot = 0;
        std::uint16_t sequence = 0;
        bool last = false;
        std::vector<std::uint8_t> bytes;
    };

    virtual ~VoiceTransport() = default;

    /// Sends a frame, unreliable. False if it cannot go now: it is dropped.
    virtual bool send(const Outgoing &frame) = 0;
    /// The frames received since the last call, in the order they came.
    [[nodiscard]] virtual std::vector<Incoming> receive() = 0;
};

/// The voice of a game carried by the game: the player of this client is on Backend::Game.
///
/// - **Out**: while it talks and is not muted, the samples of the microphone, by frames of 20 ms, go through
///   the SoundProcessing (the echo of what the Loopback says is played, the noise, the gain), are given their
///   volume, encoded and sent, each with its sequence; silence is not sent (DTX). With the voice
///   activity on, only the frames of the voice go (VoiceActivity). The microphone is open only while the
///   player is not muted: the system shows it in use only then. Without one, the player listens only.
/// - **In**: the frames of each other player go into its JitterBuffer, which gives back 20 ms at a time in
///   their order and pace; they are decoded with its own Decoder (from the copy the next frame carries for a
///   frame lost, or concealed) and written to its stream of the Playback. A player is speaking while its
///   frames played are less than 200 ms old. Deafened, the player hears none of them.
///
/// @code
/// voice::GameVoice voice(session, microphone, streams, &loopback);    // no loopback: no echo cancellation
/// voice.join({.self = accepted.slot});
/// voice.setSelfMute(false);
/// voice.setTalking(pushToTalk.talking(now));     // or setVoiceActivity(threshold) and setTalking(true)
/// voice.poll();    // each frame of the game loop
/// @endcode
///
/// Every call is made on the thread of the game loop. The transport and the devices must outlive it.
class GameVoice final : public VoiceChannel
{
public:
    using Clock = std::chrono::steady_clock;

    /// How long a player stays speaking after its last frame played.
    static constexpr std::chrono::milliseconds SpeakingHold{200};
    /// Samples of the microphone kept for the next poll() at most: 60 ms. Older ones, of a game loop that
    /// stalled, are dropped rather than sent late.
    static constexpr std::size_t MaxCaptureBacklog = 3 * FrameSamples;
    /// Under this level, in dBFS, a frame of the microphone is silence: what a microphone unplugged, or muted
    /// by a switch of its own, gives. A quiet room is about -70.
    static constexpr float SilenceLevel = -85.0f;
    /// Frames of silence while the player talks after which the microphone is said silent: 10 s.
    static constexpr std::size_t SilentFrames = 10 * FramesPerSecond;

private:
    struct Remote {
        Participant player;
        JitterBuffer jitter;
        Decoder decoder;
        std::optional<Clock::time_point> heard;
    };

    VoiceTransport &_transport;
    Capture &_capture;
    Playback &_playback;
    Loopback *_loopback;
    bool _loopbackOpen = false;
    std::optional<Join> _joined;
    std::array<std::optional<Remote>, MaxParticipants> _remotes;
    bool _selfMuted = true;
    bool _talking = false;
    bool _captureOpen = false;
    /// The microphone could not be opened, the last time it was tried.
    bool _microphoneMissing = false;
    bool _deafened = false;
    float _inputVolume = DefaultVolume;
    float _outputVolume = DefaultVolume;
    /// The threshold of the voice activity, if it is on.
    std::optional<float> _threshold;
    VoiceActivity _activity;
    float _inputLevel = SilenceFloor;
    /// Frames of silence in a row while the player talks.
    std::size_t _silence = 0;

    /// The microphone, and what is played, before the encoder: the samples not encoded yet wait there.
    SoundProcessing _processing;
    Encoder _encoder;
    /// A talk spurt is being sent: the next frame not sent is its last.
    bool _spurt = false;
    std::uint16_t _sequence = 0;
    std::size_t _sent = 0;

    [[nodiscard]] Remote *remote(Slot slot);
    [[nodiscard]] const Remote *remote(Slot slot) const;
    /// Opens or closes the microphone as the player is in a game and not muted.
    void updateCapture();
    void applyVolume(const Remote &remote);
    void capture();
    void sendFrame(ConstPcm processed, ConstPcm heard);
    void receive(Clock::time_point now);
    void play(Slot slot, Remote &remote, Clock::time_point now);

public:
    /// @param loopback What the speakers play, for the echo cancellation; nothing without it.
    GameVoice(VoiceTransport &transport, Capture &capture, Playback &playback, Loopback *loopback = nullptr);
    ~GameVoice() override;

    GameVoice(const GameVoice &) = delete;
    GameVoice &operator=(const GameVoice &) = delete;

    void join(const Join &join) override;
    void leave() override;
    void addParticipant(Slot slot) override;
    void removeParticipant(Slot slot) override;
    void setBackend(Slot slot, Backend backend) override;
    void setStatus(Slot slot, Status status) override;
    void setSelfMute(bool muted) override;
    void setTalking(bool talking) override;
    void setParticipantVolume(Slot slot, float volume) override;
    void setParticipantMute(Slot slot, bool muted) override;
    void setParticipantPosition(Slot slot, Position position) override;

    /// Sends only the frames of the voice, those over this level in dBFS and the ones just after
    /// (VoiceActivity), while the player talks; nothing for every frame while it talks.
    void setVoiceActivity(std::optional<float> threshold);
    /// The volume of the microphone, from 0 to MaxVolume: the samples are multiplied by it before they are
    /// encoded.
    void setInputVolume(float volume);
    /// What the microphone goes through, before its volume: the noise suppression and the gain control.
    void setProcessing(const ProcessingSettings &settings);
    /// The volume of every voice, from 0 to MaxVolume, times the volume of each player.
    void setOutputVolume(float volume);
    /// Deafens the player: the voices of the others are not played. It does not mute it: the client does,
    /// with setSelfMute().
    void setDeafened(bool deafened);
    [[nodiscard]] std::vector<Participant> participants() const override;
    void poll() override { poll(Clock::now()); }
    /// poll() at a time given, for the tests.
    void poll(Clock::time_point now);

    /// In the voice of a game.
    [[nodiscard]] bool joined() const { return _joined.has_value(); }
    /// The microphone is open.
    [[nodiscard]] bool microphoneOpen() const { return _captureOpen; }
    /// The microphone could not be opened the last time the player was unmuted: it listens only. Cleared when
    /// one opens.
    [[nodiscard]] bool microphoneMissing() const { return _microphoneMissing; }
    /// The voice of the player goes out now: a talk spurt is being sent. Lit in the HUD of the player.
    [[nodiscard]] bool transmitting() const { return _spurt; }
    /// The player is muted by itself (the default).
    [[nodiscard]] bool selfMuted() const { return _selfMuted; }
    /// Deafened by setDeafened().
    [[nodiscard]] bool deafened() const { return _deafened; }
    /// The level of the last frame of the microphone, in dBFS, after its volume; SilenceFloor while it is
    /// closed.
    [[nodiscard]] float inputLevel() const { return _inputLevel; }
    /// The microphone gave only silence (SilenceLevel, as it heard it, before the processing and its volume)
    /// for SilentFrames while the player talked: it is probably unplugged, or muted by a switch of its own. A
    /// frame with a sound clears it.
    [[nodiscard]] bool microphoneSilent() const { return _silence >= SilentFrames; }
    /// Frames of the player sent, since the start.
    [[nodiscard]] std::size_t sentFrames() const { return _sent; }
    /// The processing of the microphone, for its echo delay measured.
    [[nodiscard]] const SoundProcessing &processing() const { return _processing; }
};

} // namespace rtype::voice
