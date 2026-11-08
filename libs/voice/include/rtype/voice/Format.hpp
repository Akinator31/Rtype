#pragma once

#include <cstddef>

/// The format of the voice (docs/architecture/voice.md#the-codec): what the codec, the game protocol and the
/// devices agree on. Header only, without Opus: the game protocol and the server use it too.
namespace rtype::voice
{

/// The rate of the samples of the voice, captured, sent and played.
inline constexpr int SampleRate = 48000;
/// The samples of one frame: 20 ms at 48 kHz, mono.
inline constexpr std::size_t FrameSamples = 960;
/// Frames of the voice of one player in a second.
inline constexpr std::size_t FramesPerSecond = 50;
/// The largest frame the encoder gives, the game protocol carries and the decoder accepts.
inline constexpr std::size_t MaxFrameBytes = 160;

} // namespace rtype::voice
