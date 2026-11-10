#include "rtype/voice/SharedPlayback.hpp"

namespace rtype::voice
{

SharedPlayback::View::View(SharedPlayback &owner, Backend backend) : _owner(owner), _backend(backend)
{
    _volume.fill(DefaultVolume);
}

bool SharedPlayback::View::available() const
{
    return _owner._playback.available();
}

void SharedPlayback::View::open(Slot slot)
{
    if (slot >= MaxParticipants || _open[slot]) {
        return;
    }
    _open[slot] = true;
    _owner._playback.open(slot);
    if (_owner.source(slot) == _backend) {
        _owner._playback.setVolume(slot, _volume[slot]);
    }
}

void SharedPlayback::View::close(Slot slot)
{
    if (slot >= MaxParticipants || !_open[slot]) {
        return;
    }
    _open[slot] = false;
    const View &other = _backend == Backend::Discord ? _owner._game : _owner._discord;
    if (!other.isOpen(slot)) {
        _owner._playback.close(slot);
    }
}

bool SharedPlayback::View::write(Slot slot, std::span<const std::int16_t> samples)
{
    if (slot >= MaxParticipants || _owner.source(slot) != _backend) {
        // Heard from the other source: never twice
        return true;
    }
    return _owner._playback.write(slot, samples);
}

void SharedPlayback::View::setVolume(Slot slot, float volume)
{
    if (slot >= MaxParticipants) {
        return;
    }
    _volume[slot] = volume;
    if (_open[slot] && _owner.source(slot) == _backend) {
        _owner._playback.setVolume(slot, volume);
    }
}

SharedPlayback::SharedPlayback(Playback &playback) : _playback(playback)
{
    _sources.fill(Backend::Game);
}

void SharedPlayback::setSource(Slot slot, Backend backend)
{
    if (slot >= MaxParticipants) {
        return;
    }
    const Backend source = backend == Backend::Discord ? Backend::Discord : Backend::Game;
    if (source == _sources[slot]) {
        return;
    }
    _sources[slot] = source;
    if (const View &now = viewOf(source); now.isOpen(slot)) {
        _playback.setVolume(slot, now.volume(slot));
    }
}

Backend SharedPlayback::source(Slot slot) const
{
    return slot < MaxParticipants ? _sources[slot] : Backend::Game;
}

} // namespace rtype::voice
