#include "rtype/voice/EchoDelay.hpp"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace rtype::voice
{

namespace
{

/// Under this mean power of a decimated sample, a signal is silent: about -70 dBFS.
constexpr double SilentPower = 32768.0 * 32768.0 * 1e-7;

/// The samples averaged by EchoDecimation, their mean taken off: a low-pass filter enough for a correlation.
std::vector<double> decimate(std::span<const std::int16_t> samples)
{
    std::vector<double> out(samples.size() / EchoDecimation);
    double mean = 0.0;
    for (std::size_t index = 0; index < out.size(); ++index) {
        double sum = 0.0;
        for (std::size_t sample = 0; sample < EchoDecimation; ++sample) {
            sum += samples[(index * EchoDecimation) + sample];
        }
        out[index] = sum / static_cast<double>(EchoDecimation);
        mean += out[index];
    }
    mean /= static_cast<double>(std::max<std::size_t>(out.size(), 1));
    for (double &value : out) {
        value -= mean;
    }
    return out;
}

} // namespace

std::optional<EchoEstimate> measureEchoDelay(std::span<const std::int16_t> reference,
                                             std::span<const std::int16_t> microphone)
{
    if (reference.size() != EchoWindow || microphone.size() != EchoMicrophoneWindow) {
        return std::nullopt;
    }
    const std::vector<double> played = decimate(reference);
    const std::vector<double> heard = decimate(microphone);
    const std::size_t window = played.size();
    const std::size_t lags = heard.size() - window + 1;

    double playedPower = 0.0;
    for (const double value : played) {
        playedPower += value * value;
    }
    if (playedPower < SilentPower * static_cast<double>(window)) {
        return std::nullopt;
    }

    // The power of the microphone under the window at each lag, slid along
    double heardPower = 0.0;
    for (std::size_t index = 0; index < window; ++index) {
        heardPower += heard[index] * heard[index];
    }
    std::optional<EchoEstimate> best;
    for (std::size_t lag = 0; lag < lags; ++lag) {
        if (lag > 0) {
            heardPower +=
                (heard[lag + window - 1] * heard[lag + window - 1]) - (heard[lag - 1] * heard[lag - 1]);
        }
        if (heardPower < SilentPower * static_cast<double>(window)) {
            continue;
        }
        double product = 0.0;
        for (std::size_t index = 0; index < window; ++index) {
            product += played[index] * heard[lag + index];
        }
        const auto correlation = static_cast<float>(product / std::sqrt(playedPower * heardPower));
        if (!best || correlation > best->correlation) {
            best = EchoEstimate{.delay = (static_cast<std::ptrdiff_t>(lag * EchoDecimation)) -
                                         static_cast<std::ptrdiff_t>(EchoEarliest),
                                .correlation = correlation};
        }
    }
    return best;
}

bool EchoDelay::update(const EchoEstimate &estimate)
{
    if (estimate.correlation < MinCorrelation) {
        return false;
    }
    if (!_delay) {
        _delay = estimate.delay;
        return true;
    }
    if (std::abs(estimate.delay - *_delay) <= Agreement) {
        _candidate.reset();
        return false;
    }
    if (_candidate && std::abs(estimate.delay - *_candidate) <= Agreement) {
        _delay = estimate.delay;
        _candidate.reset();
        return true;
    }
    _candidate = estimate.delay;
    return false;
}

} // namespace rtype::voice
