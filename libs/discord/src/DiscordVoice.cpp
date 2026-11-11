#include "rtype/discord/DiscordVoice.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/RingBuffer.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace rtype::discord
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("voice");
    return instance;
}

/// Frames of Discord converted at once, on its audio thread.
constexpr std::size_t ConvertChunk = 256;
/// The rates of Discord taken, beyond which a voice is dropped: the buffer of a chunk holds the samples of
/// the lowest at 48 kHz.
constexpr std::int32_t LowestRate = 8000;
constexpr std::int32_t HighestRate = 192000;
constexpr std::size_t MaxChannels = 8;
/// Samples written to the playback at once.
constexpr std::size_t PlayChunk = 4 * voice::FrameSamples;

} // namespace

std::string lobbySecret(std::span<const std::byte, 32> secret)
{
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(secret.size() * 2);
    for (const std::byte byte : secret) {
        const auto value = std::to_integer<unsigned>(byte);
        text.push_back(Digits[value >> 4U]);
        text.push_back(Digits[value & 0x0FU]);
    }
    return text;
}

namespace
{

/// A voice of Discord to 48 kHz mono, on its audio thread: mixed down, then resampled linearly. Keeps where
/// the resampling is between two calls.
struct Converter {
    double phase = 0.0;
    std::int16_t previous = 0;

    /// Whether Discord may give that: beyond, the voice is dropped.
    [[nodiscard]] static bool takes(std::int32_t rate, std::size_t channels)
    {
        return channels != 0 && channels <= MaxChannels && rate >= LowestRate && rate <= HighestRate;
    }

    /// Converts `data` and writes it to `out`; what does not fit is dropped: the game loop is far behind.
    void convert(std::span<const std::int16_t> data, std::int32_t rate, std::size_t channels,
                 voice::RingBuffer<std::int16_t> &out)
    {
        const std::size_t frames = data.size() / channels;
        // Each input sample goes forward this much in the samples at 48 kHz
        const double step = static_cast<double>(rate) / voice::SampleRate;
        std::array<std::int16_t, ConvertChunk *(voice::SampleRate / LowestRate) + 1> converted{};
        for (std::size_t first = 0; first < frames; first += ConvertChunk) {
            const std::size_t count = std::min(ConvertChunk, frames - first);
            std::size_t written = 0;
            for (std::size_t frame = first; frame < first + count; ++frame) {
                int sum = 0;
                for (std::size_t channel = 0; channel < channels; ++channel) {
                    sum += data[frame * channels + channel];
                }
                const auto mono = static_cast<std::int16_t>(sum / static_cast<int>(channels));
                if (rate == voice::SampleRate) {
                    converted[written++] = mono;
                    continue;
                }
                // Linear, between the previous sample and this one
                while (phase < 1.0 && written < converted.size()) {
                    const double value = previous + (mono - previous) * phase;
                    converted[written++] = static_cast<std::int16_t>(std::lround(value));
                    phase += step;
                }
                phase -= 1.0;
                previous = mono;
            }
            (void)out.write(std::span<const std::int16_t>(converted.data(), written));
        }
    }
};

} // namespace

/// The voice of each slot and the microphone, written by the audio thread of Discord and read by the game
/// loop.
struct DiscordVoice::Inbox {
    struct Entry {
        /// The member whose voice goes there, 0 for none: written by the game loop, read by Discord.
        std::atomic<std::uint64_t> user{0};
        voice::RingBuffer<std::int16_t> samples{InboxSamples};
        /// On the thread of Discord only
        Converter converter;
    };

    /// The microphone of the player, as Discord captures it.
    struct Microphone {
        /// The voice of the game reads it: written by the game loop, read by Discord.
        std::atomic<bool> open{false};
        /// Samples Discord gave, open or not: whether its captured callback runs at all.
        std::atomic<std::uint64_t> captured{0};
        voice::RingBuffer<std::int16_t> samples{CaptureSamples};
        /// On the thread of Discord only
        Converter converter;
    };

    std::array<Entry, voice::MaxParticipants> slots;
    Microphone microphone;

    /// On the audio thread of Discord: the voice of a member to the slot it is matched to, at 48 kHz mono.
    void receive(std::uint64_t user, std::span<const std::int16_t> data, std::int32_t rate,
                 std::size_t channels)
    {
        if (user == 0 || !Converter::takes(rate, channels)) {
            return;
        }
        for (Entry &slot : slots) {
            if (slot.user.load(std::memory_order_acquire) == user) {
                slot.converter.convert(data, rate, channels, slot.samples);
                return;
            }
        }
    }

    /// On the audio thread of Discord: the microphone of the player, at 48 kHz mono, while the voice of the
    /// game reads it.
    void capture(std::span<const std::int16_t> data, std::int32_t rate, std::size_t channels)
    {
        if (!Converter::takes(rate, channels)) {
            return;
        }
        microphone.captured.fetch_add(data.size() / channels, std::memory_order_relaxed);
        if (microphone.open.load(std::memory_order_acquire)) {
            microphone.converter.convert(data, rate, channels, microphone.samples);
        }
    }
};

/// The microphone of Discord, for the voice of the game.
class DiscordVoice::Capture final : public voice::Capture
{
    std::shared_ptr<Inbox> _inbox;

public:
    explicit Capture(std::shared_ptr<Inbox> inbox) : _inbox(std::move(inbox)) {}

    /// Discord has the microphone already: always there.
    bool open() override
    {
        _inbox->microphone.samples.discard();
        _inbox->microphone.open.store(true, std::memory_order_release);
        return true;
    }

    void close() override
    {
        _inbox->microphone.open.store(false, std::memory_order_release);
        _inbox->microphone.samples.discard();
    }

    std::size_t read(std::span<std::int16_t> samples) override
    {
        return _inbox->microphone.samples.read(samples);
    }
};

DiscordVoice::DiscordVoice(VoiceCall &call, voice::Playback &playback, std::string secret,
                           std::uint32_t playerId)
    : _call(call), _playback(playback), _secret(std::move(secret)), _playerId(playerId),
      _inbox(std::make_shared<Inbox>()), _capture(std::make_unique<Capture>(_inbox))
{
}

DiscordVoice::~DiscordVoice()
{
    leave();
}

DiscordVoice::Remote *DiscordVoice::remote(voice::Slot slot)
{
    return slot < _remotes.size() && _remotes[slot] ? &*_remotes[slot] : nullptr;
}

void DiscordVoice::join(const voice::Join &join)
{
    if (_joined) {
        leave();
    }
    _joined = join;
    if (join.self < _remotes.size()) {
        _remotes[join.self].reset();
    }
    logger().info("Joining the voice of Discord as player {}", _playerId);
    _call.start(
        _secret, {{"player", std::to_string(_playerId)}},
        [inbox = _inbox](std::uint64_t user, std::span<const std::int16_t> samples, std::int32_t rate,
                         std::size_t channels) { inbox->receive(user, samples, rate, channels); },
        [inbox = _inbox](std::span<const std::int16_t> samples, std::int32_t rate, std::size_t channels) {
            inbox->capture(samples, rate, channels);
        });
    _call.setSelfMute(_selfMuted);
    _call.setSelfDeaf(_deafened);
    _call.setInputVolume(std::min(_inputVolume, 1.0f));
    _call.setProcessing(_processing.noiseSuppression, _processing.gainControl);
    applyTalk();
}

void DiscordVoice::leave()
{
    if (!_joined) {
        return;
    }
    _call.end();
    closeStreams();
    for (Inbox::Entry &entry : _inbox->slots) {
        entry.user.store(0, std::memory_order_release);
    }
    _joined.reset();
}

void DiscordVoice::addParticipant(voice::Slot slot)
{
    if (slot >= _remotes.size() || (_joined && slot == _joined->self) || _remotes[slot]) {
        return;
    }
    _remotes[slot].emplace(Remote{.player = {.slot = slot}, .playerId = {}});
    if (_streamsOpen) {
        _playback.open(slot);
        applyVolume(*_remotes[slot]);
    }
}

void DiscordVoice::removeParticipant(voice::Slot slot)
{
    if (remote(slot) == nullptr) {
        return;
    }
    if (_streamsOpen) {
        _playback.close(slot);
    }
    _inbox->slots[slot].user.store(0, std::memory_order_release);
    _remotes[slot].reset();
}

void DiscordVoice::setPlayerId(voice::Slot slot, std::uint32_t playerId)
{
    if (Remote *other = remote(slot)) {
        other->playerId = playerId;
    }
}

void DiscordVoice::setBackend(voice::Slot slot, voice::Backend backend)
{
    if (Remote *other = remote(slot)) {
        other->player.backend = backend;
    }
}

void DiscordVoice::setStatus(voice::Slot slot, voice::Status status)
{
    if (Remote *other = remote(slot)) {
        other->player.status = status;
        applyVolume(*other);
    }
}

void DiscordVoice::setSelfMute(bool muted)
{
    if (muted != _selfMuted) {
        _selfMuted = muted;
        if (_joined) {
            _call.setSelfMute(muted);
        }
    }
}

void DiscordVoice::setTalking(bool talking)
{
    if (talking != _talking) {
        _talking = talking;
        if (_joined && !_threshold) {
            _call.setPushToTalk(talking);
        }
    }
}

void DiscordVoice::setVoiceActivity(std::optional<float> threshold)
{
    if (threshold != _threshold) {
        _threshold = threshold;
        if (_joined) {
            applyTalk();
        }
    }
}

void DiscordVoice::applyTalk()
{
    if (_threshold) {
        _call.setTalkMode(CallTalkMode::VoiceActivity, *_threshold);
    } else {
        _call.setTalkMode(CallTalkMode::PushToTalk, 0.0f);
        _call.setPushToTalk(_talking);
    }
}

void DiscordVoice::setInputVolume(float volume)
{
    const float clamped = std::clamp(volume, 0.0f, voice::MaxVolume);
    if (clamped != _inputVolume) {
        _inputVolume = clamped;
        if (_joined) {
            _call.setInputVolume(std::min(clamped, 1.0f));
        }
    }
}

void DiscordVoice::setProcessing(const voice::ProcessingSettings &settings)
{
    if (settings != _processing) {
        _processing = settings;
        if (_joined) {
            _call.setProcessing(settings.noiseSuppression, settings.gainControl);
        }
    }
}

void DiscordVoice::setOutputVolume(float volume)
{
    const float clamped = std::clamp(volume, 0.0f, voice::MaxVolume);
    if (clamped != _outputVolume) {
        _outputVolume = clamped;
        for (const std::optional<Remote> &other : _remotes) {
            if (other) {
                applyVolume(*other);
            }
        }
    }
}

void DiscordVoice::setDeafened(bool deafened)
{
    if (deafened == _deafened) {
        return;
    }
    _deafened = deafened;
    if (_joined) {
        _call.setSelfDeaf(deafened);
    }
    for (const std::optional<Remote> &other : _remotes) {
        if (other) {
            applyVolume(*other);
        }
    }
}

void DiscordVoice::setParticipantVolume(voice::Slot slot, float volume)
{
    Remote *other = remote(slot);
    const float clamped = std::clamp(volume, 0.0f, voice::MaxVolume);
    if (other != nullptr && other->player.volume != clamped) {
        other->player.volume = clamped;
        applyVolume(*other);
    }
}

void DiscordVoice::setParticipantMute(voice::Slot slot, bool muted)
{
    if (Remote *other = remote(slot); other != nullptr && other->player.mutedByMe != muted) {
        other->player.mutedByMe = muted;
        applyVolume(*other);
    }
}

void DiscordVoice::setParticipantPosition(voice::Slot /*slot*/, voice::Position /*position*/)
{
    // Heard from where the ship is with the spatial voice of the 3D, to come
}

bool DiscordVoice::heard(const Remote &remote) const
{
    return !remote.player.mutedByMe && !remote.player.status.mutedByAdmin && !_deafened;
}

void DiscordVoice::applyVolume(const Remote &remote)
{
    if (_streamsOpen) {
        _playback.setVolume(remote.player.slot,
                            heard(remote) ? std::min(remote.player.volume * _outputVolume, voice::MaxVolume)
                                          : 0.0f);
    }
}

std::vector<voice::Participant> DiscordVoice::participants() const
{
    std::vector<voice::Participant> players;
    for (const std::optional<Remote> &other : _remotes) {
        if (other) {
            players.push_back(other->player);
        }
    }
    return players;
}

voice::Capture &DiscordVoice::capture()
{
    return *_capture;
}

std::uint64_t DiscordVoice::capturedSamples() const
{
    return _inbox->microphone.captured.load(std::memory_order_relaxed);
}

bool DiscordVoice::connected() const
{
    return _joined && _call.status() == CallStatus::Connected;
}

bool DiscordVoice::failed() const
{
    return _joined && _call.status() == CallStatus::Failed;
}

bool DiscordVoice::transmitting() const
{
    return connected() && !_selfMuted && _call.speaking();
}

void DiscordVoice::openStreams()
{
    if (_streamsOpen || !_playback.available()) {
        return;
    }
    _streamsOpen = true;
    for (const std::optional<Remote> &other : _remotes) {
        if (other) {
            _playback.open(other->player.slot);
            applyVolume(*other);
        }
    }
}

void DiscordVoice::closeStreams()
{
    if (!_streamsOpen) {
        return;
    }
    _streamsOpen = false;
    for (std::optional<Remote> &other : _remotes) {
        if (other) {
            _playback.close(other->player.slot);
            other->player.speaking = false;
        }
    }
}

void DiscordVoice::poll()
{
    if (!_joined) {
        return;
    }
    if (!connected()) {
        // Lost, or not there yet: the voice of the game may need the streams
        closeStreams();
        return;
    }
    openStreams();
    match();
    play();
}

void DiscordVoice::match()
{
    const std::vector<CallMember> members = _call.members();
    for (std::optional<Remote> &other : _remotes) {
        if (!other) {
            continue;
        }
        const CallMember *found = nullptr;
        if (other->playerId) {
            const std::string id = std::to_string(*other->playerId);
            for (const CallMember &member : members) {
                const auto player = member.metadata.find("player");
                if (player != member.metadata.end() && player->second == id) {
                    found = &member;
                    break;
                }
            }
        }
        Inbox::Entry &entry = _inbox->slots[other->player.slot];
        const std::uint64_t user = found != nullptr ? found->userId : 0;
        if (entry.user.load(std::memory_order_relaxed) != user) {
            entry.user.store(user, std::memory_order_release);
            if (user != 0) {
                logger().info("The member {} of the Discord lobby is the player in slot {}", user,
                              other->player.slot);
            }
        }
        other->player.speaking = found != nullptr && found->speaking && heard(*other);
    }
}

void DiscordVoice::play()
{
    std::array<std::int16_t, PlayChunk> chunk{};
    for (std::optional<Remote> &other : _remotes) {
        if (!other) {
            continue;
        }
        Inbox::Entry &entry = _inbox->slots[other->player.slot];
        if (!heard(*other)) {
            entry.samples.discard();
            continue;
        }
        while (const std::size_t count = entry.samples.read(chunk)) {
            (void)_playback.write(other->player.slot, std::span<const std::int16_t>(chunk.data(), count));
            if (count < chunk.size()) {
                break;
            }
        }
    }
}

} // namespace rtype::discord
