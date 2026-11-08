#include "rtype/voice/JitterBuffer.hpp"

#include <algorithm>
#include <utility>

namespace rtype::voice
{

std::int64_t JitterBuffer::unwrap(std::uint16_t sequence) const
{
    if (!_newest) {
        return sequence;
    }
    // The nearest sequence to the newest one with these 16 bits
    const auto difference = static_cast<std::int16_t>(static_cast<std::uint16_t>(sequence - *_newest));
    return *_newest + difference;
}

void JitterBuffer::push(Frame frame, Clock::time_point arrival)
{
    const std::int64_t sequence = unwrap(frame.sequence);
    if (_playing && sequence >= _next + static_cast<std::int64_t>(Capacity)) {
        // Far ahead: the sender went on while this client heard nothing. A new spurt
        _frames.clear();
        _playing = false;
        _played.reset();
    }
    if (_playing && _started && sequence < _next) {
        // Late if it was made up, received twice if it was played
        if (std::erase(_missed, sequence) > 0) {
            arrivedLate();
        }
        return;
    }
    if (!_playing && _played && sequence <= *_played) {
        // Of a spurt played and over
        return;
    }
    if (_frames.contains(sequence)) {
        return;
    }
    _newest = std::max(_newest.value_or(sequence), sequence);
    _frames.emplace(sequence, std::move(frame));
    if (!_playing) {
        start(arrival + _delay);
    } else if (!_started) {
        // Before the first frame of the spurt: an earlier one came after it
        _next = std::min(_next, sequence);
    }
}

void JitterBuffer::start(Clock::time_point at)
{
    _playing = true;
    _started = false;
    _next = _frames.begin()->first;
    _playAt = at;
    _concealed = 0;
}

void JitterBuffer::stop()
{
    _playing = false;
    // Frames of the next spurt already there: played on, in the same time
    if (_played) {
        std::erase_if(_frames, [this](const auto &entry) { return entry.first <= *_played; });
    }
    _missed.clear();
    if (!_frames.empty()) {
        start(_playAt);
    }
}

void JitterBuffer::arrivedLate()
{
    ++_late;
    _steady = 0;
    if (_delay < MaxDelay) {
        _delay += DelayStep;
        // This spurt too: 20 ms more to wait for the next frames
        _playAt += DelayStep;
    }
}

std::optional<JitterBuffer::Output> JitterBuffer::pop(Clock::time_point now)
{
    if (!_playing || now < _playAt) {
        return std::nullopt;
    }
    if (now - _playAt > MaxDelay) {
        // Stalled: what should have played already is dropped, not played all at once
        while (now - _playAt >= FrameTime) {
            _frames.erase(_next);
            _played = _next;
            ++_next;
            _playAt += FrameTime;
        }
    }
    _started = true;
    _playAt += FrameTime;

    if (auto frame = _frames.find(_next); frame != _frames.end()) {
        Output output{Kind::Frame, std::move(frame->second.bytes)};
        const bool last = frame->second.last;
        _frames.erase(frame);
        _played = _next;
        ++_next;
        _concealed = 0;
        if (++_steady >= SteadyFrames) {
            _steady = 0;
            _delay = std::max(_delay - DelayStep, MinDelay);
        }
        if (last) {
            stop();
        }
        return output;
    }

    if (_concealed >= MaxConcealed) {
        stop();
        return std::nullopt;
    }
    ++_concealed;
    _missed.push_back(_next);
    if (_missed.size() > Capacity) {
        _missed.erase(_missed.begin());
    }
    _played = _next;
    ++_next;
    if (const auto next = _frames.find(_next); next != _frames.end()) {
        return Output{Kind::Fec, next->second.bytes};
    }
    return Output{Kind::Conceal, {}};
}

void JitterBuffer::reset()
{
    *this = JitterBuffer{};
}

} // namespace rtype::voice
