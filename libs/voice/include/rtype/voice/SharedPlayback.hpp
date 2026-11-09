#pragma once

#include "rtype/voice/Backend.hpp"
#include "rtype/voice/Devices.hpp"
#include "rtype/voice/VoiceChannel.hpp"

#include <array>
#include <span>

/// One Playback for the two voices of a mixed game (docs/architecture/voice.md#mixed-games).
namespace rtype::voice
{

/// The Playback of the client, shared by the voice of the game and the one of Discord, which run together
/// for a player on Discord in a game where others are on the game: each slot is heard from **one source**,
/// the one setSource() gives (the roster), and what the other one writes for it is dropped. Nobody is heard
/// twice, even while a player changes backend and frames of both still come.
///
/// - **The streams**: a stream is open while either voice has it open, and closed when both closed it.
/// - **The volumes**: each voice gives its own; the stream has the one of its source, given again when the
///   source changes.
///
/// @code
/// voice::SharedPlayback playbacks(streams);
/// voice::GameVoice game(transport, microphone, playbacks.game());
/// discord::DiscordVoice discord(call, playbacks.discord(), secret, playerId);
/// playbacks.setSource(slot, voice::Backend::Discord);    // heard through Discord from now on
/// @endcode
///
/// Every call is made on the thread of the game loop. The Playback must outlive it, and it the two voices.
class SharedPlayback
{
    class View final : public Playback
    {
        SharedPlayback &_owner;
        Backend _backend;
        std::array<bool, MaxParticipants> _open{};
        std::array<float, MaxParticipants> _volume{};

    public:
        View(SharedPlayback &owner, Backend backend);

        [[nodiscard]] bool available() const override;
        void open(Slot slot) override;
        void close(Slot slot) override;
        /// Dropped, and said written, when the slot is heard from the other source.
        bool write(Slot slot, std::span<const std::int16_t> samples) override;
        void setVolume(Slot slot, float volume) override;

        [[nodiscard]] bool isOpen(Slot slot) const { return _open[slot]; }
        [[nodiscard]] float volume(Slot slot) const { return _volume[slot]; }
    };

    Playback &_playback;
    View _game{*this, Backend::Game};
    View _discord{*this, Backend::Discord};
    std::array<Backend, MaxParticipants> _sources{};

    [[nodiscard]] View &viewOf(Backend backend) { return backend == Backend::Discord ? _discord : _game; }

public:
    /// Every slot heard from the voice of the game until setSource() says otherwise.
    explicit SharedPlayback(Playback &playback);

    SharedPlayback(const SharedPlayback &) = delete;
    SharedPlayback &operator=(const SharedPlayback &) = delete;

    /// What the voice of the game plays through.
    [[nodiscard]] Playback &game() { return _game; }
    /// What the voice of Discord plays through.
    [[nodiscard]] Playback &discord() { return _discord; }

    /// Where the voice of a slot is heard from: Backend::Discord through the voice of Discord, anything else
    /// through the voice of the game. The stream takes the volume its new source gave it.
    void setSource(Slot slot, Backend backend);
    /// Where the voice of a slot is heard from: Backend::Game or Backend::Discord.
    [[nodiscard]] Backend source(Slot slot) const;
};

} // namespace rtype::voice
