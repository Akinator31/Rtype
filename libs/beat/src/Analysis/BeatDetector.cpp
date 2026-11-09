#include "rtype/beat/Analysis/BeatDetector.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <utility>

namespace rtype::beat
{

namespace
{

constexpr std::size_t FrameSize = 2048;
constexpr std::size_t HopSize = 512;
/// Frames on each side of a frame used to tell what is usual around it (about half a second).
constexpr std::size_t ContextFrames = 43;
/// Frames on each side that must be lower than a peak.
constexpr std::size_t PeakFrames = 3;
/// Attacks of two bands this many frames apart or less are the same attack (about 35 ms).
constexpr std::size_t SameAttackFrames = 3;
/// A rise must be this many times the usual one to count.
constexpr float Sensitivity = 1.6f;
/// Below this rise, a band is silent.
constexpr float MinRise = 0.08f;

struct BandRange {
    float lowHertz;
    float highHertz;
};

constexpr std::array<BandRange, BeatBandCount> BandRanges{
    {{40.0f, 150.0f}, {150.0f, 2500.0f}, {2500.0f, 10000.0f}}};

using Spectrum = std::vector<std::complex<float>>;

/// In-place radix-2 Fourier transform.
void transform(Spectrum &values)
{
    const std::size_t size = values.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const float angle = -2.0f * std::numbers::pi_v<float> / static_cast<float>(length);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < size; start += length) {
            std::complex<float> twiddle(1.0f, 0.0f);
            for (std::size_t k = 0; k < length / 2; ++k) {
                const std::complex<float> even = values[start + k];
                const std::complex<float> odd = values[start + k + length / 2] * twiddle;
                values[start + k] = even + odd;
                values[start + k + length / 2] = even - odd;
                twiddle *= step;
            }
        }
    }
}

std::vector<float> makeWindow()
{
    std::vector<float> window(FrameSize);
    for (std::size_t i = 0; i < FrameSize; ++i) {
        window[i] = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) /
                                           static_cast<float>(FrameSize));
    }
    return window;
}

/// The bins of the spectrum of each band, [first, last).
std::array<std::pair<std::size_t, std::size_t>, BeatBandCount> makeBins(std::uint32_t sampleRate)
{
    std::array<std::pair<std::size_t, std::size_t>, BeatBandCount> bins{};
    const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(FrameSize);
    for (std::size_t band = 0; band < BeatBandCount; ++band) {
        const auto first = static_cast<std::size_t>(BandRanges[band].lowHertz / binWidth);
        const auto last = static_cast<std::size_t>(BandRanges[band].highHertz / binWidth);
        bins[band] = {std::max<std::size_t>(first, 1), std::min(std::max(last, first + 1), FrameSize / 2)};
    }
    return bins;
}

/// For each band, the loudness (a log of the amplitude, averaged over its bins) of every frame.
std::array<std::vector<float>, BeatBandCount> measureBands(const MonoSound &sound)
{
    const std::vector<float> window = makeWindow();
    const auto bins = makeBins(sound.sampleRate);
    const std::size_t frameCount =
        sound.samples.size() < FrameSize ? 0 : (sound.samples.size() - FrameSize) / HopSize + 1;
    std::array<std::vector<float>, BeatBandCount> loudness;
    for (auto &band : loudness) {
        band.resize(frameCount);
    }
    Spectrum spectrum(FrameSize);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        for (std::size_t i = 0; i < FrameSize; ++i) {
            spectrum[i] = sound.samples[frame * HopSize + i] * window[i];
        }
        transform(spectrum);
        for (std::size_t band = 0; band < BeatBandCount; ++band) {
            float sum = 0.0f;
            for (std::size_t bin = bins[band].first; bin < bins[band].second; ++bin) {
                sum += std::log1p(std::abs(spectrum[bin]));
            }
            loudness[band][frame] = sum / static_cast<float>(bins[band].second - bins[band].first);
        }
    }
    return loudness;
}

/// How much louder each frame is than the previous one, 0 if it is not.
std::vector<float> measureRise(const std::vector<float> &loudness)
{
    std::vector<float> rise(loudness.size(), 0.0f);
    for (std::size_t frame = 1; frame < loudness.size(); ++frame) {
        rise[frame] = std::max(0.0f, loudness[frame] - loudness[frame - 1]);
    }
    return rise;
}

float averageAround(const std::vector<float> &rise, std::size_t frame)
{
    const std::size_t first = frame > ContextFrames ? frame - ContextFrames : 0;
    const std::size_t last = std::min(rise.size(), frame + ContextFrames + 1);
    float sum = 0.0f;
    for (std::size_t i = first; i < last; ++i) {
        sum += rise[i];
    }
    return sum / static_cast<float>(last - first);
}

bool isPeak(const std::vector<float> &rise, std::size_t frame)
{
    const std::size_t first = frame > PeakFrames ? frame - PeakFrames : 0;
    const std::size_t last = std::min(rise.size(), frame + PeakFrames + 1);
    for (std::size_t i = first; i < last; ++i) {
        if (i != frame && (rise[i] > rise[frame] || (rise[i] == rise[frame] && i < frame))) {
            return false;
        }
    }
    return true;
}

float timeOfFrame(std::size_t frame, std::uint32_t sampleRate)
{
    return static_cast<float>(frame * HopSize + FrameSize / 2) / static_cast<float>(sampleRate);
}

/// A peak of the rise of a band, before the beats are chosen among the bands.
struct Candidate {
    std::size_t frame;
    BeatBand band;
    /// How many times the usual rise around it: the higher, the clearer the attack.
    float prominence;
};

std::vector<Candidate> findCandidates(const std::vector<float> &loudness, BeatBand band)
{
    const std::vector<float> rise = measureRise(loudness);
    std::vector<Candidate> candidates;
    for (std::size_t frame = 0; frame < rise.size(); ++frame) {
        const float usual = averageAround(rise, frame);
        if (rise[frame] >= MinRise && rise[frame] >= Sensitivity * usual && isPeak(rise, frame)) {
            candidates.push_back({frame, band, rise[frame] / std::max(usual, 1e-6f)});
        }
    }
    return candidates;
}

/// An attack is heard in several bands at once (a kick drum has a click): only the clearest band keeps it.
std::vector<Candidate> keepClearestBand(std::vector<Candidate> candidates)
{
    std::ranges::stable_sort(
        candidates, [](const Candidate &a, const Candidate &b) { return a.prominence > b.prominence; });
    std::vector<Candidate> kept;
    for (const Candidate &candidate : candidates) {
        const bool shadowed = std::ranges::any_of(kept, [&](const Candidate &other) {
            const std::size_t distance =
                candidate.frame > other.frame ? candidate.frame - other.frame : other.frame - candidate.frame;
            return distance <= SameAttackFrames;
        });
        if (!shadowed) {
            kept.push_back(candidate);
        }
    }
    std::ranges::sort(kept, [](const Candidate &a, const Candidate &b) { return a.frame < b.frame; });
    return kept;
}

} // namespace

std::vector<Beat> detectBeats(const MonoSound &sound, const BeatGaps &gaps)
{
    const auto loudness = measureBands(sound);
    std::vector<Candidate> candidates;
    for (std::size_t band = 0; band < BeatBandCount; ++band) {
        const std::vector<Candidate> found = findCandidates(loudness[band], static_cast<BeatBand>(band));
        candidates.insert(candidates.end(), found.begin(), found.end());
    }
    std::vector<Beat> beats;
    std::array<float, BeatBandCount> lastTime{};
    std::array<bool, BeatBandCount> hasBeat{};
    for (const Candidate &candidate : keepClearestBand(std::move(candidates))) {
        const auto band = static_cast<std::size_t>(candidate.band);
        const float time = timeOfFrame(candidate.frame, sound.sampleRate);
        if (hasBeat[band] && time - lastTime[band] < gaps[band]) {
            continue;
        }
        hasBeat[band] = true;
        lastTime[band] = time;
        beats.push_back({time, candidate.band});
    }
    return beats;
}

} // namespace rtype::beat
