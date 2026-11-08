#include "rtype/voice/GameVoice.hpp"

#include "rtype/log/Logger.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace rtype::voice
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("voice");
    return instance;
}

/// Samples read from the microphone at once.
constexpr std::size_t ReadChunk = 4 * FrameSamples;

} // namespace

GameVoice::GameVoice(VoiceTransport &transport, Capture &capture, Playback &playback)
    : _transport(transport), _capture(capture), _playback(playback)
{
}

GameVoice::~GameVoice()
{
    leave();
}

GameVoice::Remote *GameVoice::remote(Slot slot)
{
    return slot < _remotes.size() && _remotes[slot] ? &*_remotes[slot] : nullptr;
}

const GameVoice::Remote *GameVoice::remote(Slot slot) const
{
    return slot < _remotes.size() && _remotes[slot] ? &*_remotes[slot] : nullptr;
}

void GameVoice::join(const Join &join)
{
    if (_joined) {
        leave();
    }
    _joined = join;
    // Known before the game told the slot of this client: not one of the others
    if (join.self < _remotes.size()) {
        _remotes[join.self].reset();
    }
    for (Slot slot = 0; slot < _remotes.size(); ++slot) {
        if (const Remote *other = remote(slot)) {
            _playback.open(slot);
            applyVolume(*other);
        }
    }
    updateCapture();
}

void GameVoice::leave()
{
    if (!_joined) {
        return;
    }
    for (Slot slot = 0; slot < _remotes.size(); ++slot) {
        if (_remotes[slot]) {
            _playback.close(slot);
            _remotes[slot].reset();
        }
    }
    _joined.reset();
    updateCapture();
    _spurt = false;
}

void GameVoice::addParticipant(Slot slot)
{
    if (slot >= _remotes.size() || (_joined && slot == _joined->self) || _remotes[slot]) {
        return;
    }
    _remotes[slot].emplace(Remote{.player = {.slot = slot}, .jitter = {}, .decoder = {}, .heard = {}});
    if (_joined) {
        _playback.open(slot);
        applyVolume(*_remotes[slot]);
    }
}

void GameVoice::removeParticipant(Slot slot)
{
    if (remote(slot) == nullptr) {
        return;
    }
    if (_joined) {
        _playback.close(slot);
    }
    _remotes[slot].reset();
}

void GameVoice::setBackend(Slot slot, Backend backend)
{
    if (Remote *other = remote(slot)) {
        other->player.backend = backend;
        if (!carriedByGame(backend, Backend::Game)) {
            other->jitter.reset();
            other->player.speaking = false;
        }
    }
}

void GameVoice::setStatus(Slot slot, Status status)
{
    if (Remote *other = remote(slot)) {
        other->player.status = status;
    }
}

void GameVoice::setSelfMute(bool muted)
{
    if (muted == _selfMuted) {
        return;
    }
    _selfMuted = muted;
    updateCapture();
}

void GameVoice::setTalking(bool talking)
{
    _talking = talking;
}

void GameVoice::setParticipantVolume(Slot slot, float volume)
{
    Remote *other = remote(slot);
    const float clamped = std::clamp(volume, 0.0f, MaxVolume);
    if (other != nullptr && other->player.volume != clamped) {
        other->player.volume = clamped;
        applyVolume(*other);
    }
}

void GameVoice::setParticipantMute(Slot slot, bool muted)
{
    if (Remote *other = remote(slot); other != nullptr && other->player.mutedByMe != muted) {
        other->player.mutedByMe = muted;
        if (muted) {
            other->jitter.reset();
            other->player.speaking = false;
        }
        applyVolume(*other);
    }
}

void GameVoice::setVoiceActivity(std::optional<float> threshold)
{
    if (threshold != _threshold) {
        _threshold = threshold;
        _activity.reset();
    }
}

void GameVoice::setInputVolume(float volume)
{
    _inputVolume = std::clamp(volume, 0.0f, MaxVolume);
}

void GameVoice::setOutputVolume(float volume)
{
    _outputVolume = std::clamp(volume, 0.0f, MaxVolume);
    for (const std::optional<Remote> &other : _remotes) {
        if (other) {
            applyVolume(*other);
        }
    }
}

void GameVoice::setDeafened(bool deafened)
{
    if (deafened == _deafened) {
        return;
    }
    _deafened = deafened;
    for (std::optional<Remote> &other : _remotes) {
        if (other) {
            if (deafened) {
                other->jitter.reset();
                other->player.speaking = false;
                other->heard.reset();
            }
            applyVolume(*other);
        }
    }
}

void GameVoice::setParticipantPosition(Slot /*slot*/, Position /*position*/)
{
    // Heard from where the ship is with the spatial voice, to come
}

std::vector<Participant> GameVoice::participants() const
{
    std::vector<Participant> players;
    for (const std::optional<Remote> &other : _remotes) {
        if (other) {
            players.push_back(other->player);
        }
    }
    return players;
}

void GameVoice::applyVolume(const Remote &remote)
{
    if (_joined) {
        const bool silent = remote.player.mutedByMe || _deafened;
        _playback.setVolume(remote.player.slot,
                            silent ? 0.0f : std::min(remote.player.volume * _outputVolume, MaxVolume));
    }
}

void GameVoice::updateCapture()
{
    const bool wanted = _joined && !_selfMuted;
    if (wanted && !_captureOpen) {
        _captureOpen = _capture.open();
        if (!_captureOpen && !_microphoneMissing) {
            logger().warning("No microphone: the player listens only");
        }
        _microphoneMissing = !_captureOpen;
    } else if (!wanted && _captureOpen) {
        _capture.close();
        _captureOpen = false;
        _captured.clear();
        _spurt = false;
        _activity.reset();
        _inputLevel = SilenceFloor;
        _silence = 0;
    }
}

void GameVoice::poll(Clock::time_point now)
{
    if (!_joined) {
        return;
    }
    capture();
    receive(now);
    for (Slot slot = 0; slot < _remotes.size(); ++slot) {
        if (Remote *other = remote(slot)) {
            play(slot, *other, now);
        }
    }
}

void GameVoice::capture()
{
    if (!_captureOpen) {
        return;
    }
    std::array<std::int16_t, ReadChunk> chunk{};
    while (const std::size_t count = _capture.read(chunk)) {
        _captured.insert(_captured.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
        if (count < chunk.size()) {
            break;
        }
    }
    if (_captured.size() > MaxCaptureBacklog) {
        // Whole frames only, the newest kept
        const std::size_t extra = (_captured.size() - MaxCaptureBacklog + FrameSamples - 1) / FrameSamples;
        _captured.erase(_captured.begin(),
                        _captured.begin() + static_cast<std::ptrdiff_t>(extra * FrameSamples));
    }
    std::size_t used = 0;
    while (_captured.size() - used >= FrameSamples) {
        sendFrame(ConstPcm(_captured.data() + used, FrameSamples));
        used += FrameSamples;
    }
    _captured.erase(_captured.begin(), _captured.begin() + static_cast<std::ptrdiff_t>(used));
}

void GameVoice::sendFrame(ConstPcm captured)
{
    std::array<std::int16_t, FrameSamples> samples{};
    constexpr float lowest = std::numeric_limits<std::int16_t>::min();
    constexpr float highest = std::numeric_limits<std::int16_t>::max();
    for (std::size_t index = 0; index < FrameSamples; ++index) {
        const float sample = std::round(static_cast<float>(captured[index]) * _inputVolume);
        samples[index] = static_cast<std::int16_t>(std::clamp(sample, lowest, highest));
    }
    _inputLevel = levelOf(samples);

    const bool allowed = _talking && !_selfMuted;
    if (_inputLevel >= SilenceLevel) {
        _silence = 0;
    } else if (allowed) {
        ++_silence;
    }
    bool talking = allowed;
    if (!allowed) {
        _activity.reset();
    } else if (_threshold) {
        talking = _activity.update(_inputLevel, *_threshold);
    }
    if (!talking && !_spurt) {
        return;
    }
    if (talking && !_spurt) {
        _spurt = true;
        _encoder.reset();
    }
    const EncodedFrame frame = _encoder.encode(samples);
    const std::uint16_t sequence = _sequence++;
    if (!talking) {
        // The end of the spurt: its last frame, for the others to stop at once
        _spurt = false;
        if (frame.size > 0 && _transport.send({sequence, true, frame.data()})) {
            ++_sent;
        }
        return;
    }
    if (!frame.silence() && _transport.send({sequence, false, frame.data()})) {
        ++_sent;
    }
}

void GameVoice::receive(Clock::time_point now)
{
    for (VoiceTransport::Incoming &frame : _transport.receive()) {
        Remote *other = remote(frame.slot);
        if (other == nullptr || other->player.mutedByMe || _deafened ||
            !carriedByGame(other->player.backend, Backend::Game)) {
            continue;
        }
        other->jitter.push({.sequence = frame.sequence, .last = frame.last, .bytes = std::move(frame.bytes)},
                           now);
    }
}

void GameVoice::play(Slot slot, Remote &remote, Clock::time_point now)
{
    std::array<std::int16_t, FrameSamples> samples{};
    while (const std::optional<JitterBuffer::Output> out = remote.jitter.pop(now)) {
        bool decoded = false;
        switch (out->kind) {
        case JitterBuffer::Kind::Frame:
            decoded = remote.decoder.decode(out->bytes, samples);
            if (decoded) {
                remote.heard = now;
            }
            break;
        case JitterBuffer::Kind::Fec:
            decoded = remote.decoder.decodeFec(out->bytes, samples);
            break;
        case JitterBuffer::Kind::Conceal:
            break;
        }
        if (!decoded) {
            remote.decoder.conceal(samples);
        }
        if (_playback.available()) {
            (void)_playback.write(slot, samples);
        }
    }
    remote.player.speaking = remote.heard && now - *remote.heard < SpeakingHold;
}

} // namespace rtype::voice
