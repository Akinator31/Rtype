#pragma once

#include "rtype/beat/Decoder/Mp3Decoder.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace rtype::beat
{

/// The part of the sound a beat is heard in: the kick drum (bass), the snare and the voices (mid), the
/// cymbals (high).
enum class BeatBand : std::uint8_t { Bass, Mid, High };

inline constexpr std::size_t BeatBandCount = 3;

/// An attack of the music.
struct Beat {
    /// Seconds from the start of the music.
    float time = 0.0f;
    BeatBand band = BeatBand::Bass;
};

/// How the beats of each band are kept: no two beats of a band closer than `minGap` seconds, indexed by
/// BeatBand.
using BeatGaps = std::array<float, BeatBandCount>;

/// The attacks of the sound in each band, by increasing time. A band is a peak of the rise of its energy
/// above what it was just before and around. Same result for the same sound on every run.
[[nodiscard]] std::vector<Beat> detectBeats(const MonoSound &sound, const BeatGaps &gaps);

} // namespace rtype::beat
