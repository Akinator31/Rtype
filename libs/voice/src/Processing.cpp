#include "rtype/voice/Processing.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/voice/EchoDelay.hpp"

#include <rnnoise.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rtype::voice
{

namespace
{

const log::Logger &logger()
{
    static const log::Logger instance("voice");
    return instance;
}

constexpr int Half = static_cast<int>(SoundProcessing::HalfFrame);

/// The samples of each stream kept: 3 s, more than a measure of the delay compares, and than the backlog of
/// a game loop that stalls.
constexpr std::size_t HistorySamples = 3 * static_cast<std::size_t>(SampleRate);

/// What the AGC aims for, the mean level of a voice: about -12 dBFS. The most it raises a quiet voice: 20 dB,
/// so that the silence between words does not become the noise of the room raised.
constexpr float AgcLevel = 8000.0f;
constexpr spx_int32_t AgcMaxGain = 20;
/// speex_preprocess cuts what is left of the echo only with its noise suppression on: on, but it takes out
/// 1 dB of noise at most, RNNoise doing the noise
constexpr spx_int32_t SpeexNoiseSuppress = -1;

/// The samples counted from open, in a ring of HistorySamples.
struct History {
    std::vector<std::int16_t> samples = std::vector<std::int16_t>(HistorySamples);
    std::int64_t count = 0;

    void push(std::span<const std::int16_t> added)
    {
        for (const std::int16_t sample : added) {
            samples[static_cast<std::size_t>(count) % samples.size()] = sample;
            ++count;
        }
    }

    /// Whether the sample of this index is still kept.
    [[nodiscard]] bool has(std::int64_t index) const
    {
        return index >= 0 && index < count && count - index <= static_cast<std::int64_t>(samples.size());
    }

    /// The samples from this index, silence for those not kept, or not come yet.
    void copy(std::int64_t from, std::span<std::int16_t> out) const
    {
        for (std::size_t offset = 0; offset < out.size(); ++offset) {
            const std::int64_t index = from + static_cast<std::int64_t>(offset);
            out[offset] = has(index) ? samples[static_cast<std::size_t>(index) % samples.size()] : 0;
        }
    }
};

std::int16_t toSample(float value)
{
    constexpr float lowest = std::numeric_limits<std::int16_t>::min();
    constexpr float highest = std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>(std::clamp(std::round(value), lowest, highest));
}

} // namespace

struct SoundProcessing::State {
    ProcessingSettings settings;
    SpeexEchoState *echo = nullptr;
    SpeexPreprocessState *preprocess = nullptr;
    DenoiseState *denoise = nullptr;

    History microphone;
    History reference;
    /// The samples of the microphone processed or dropped: the next frame starts there.
    std::int64_t processed = 0;
    /// The reference of the sample m of the microphone is m + pairing - offset: the newest of the two
    /// streams when they were paired, then the delay of the echo.
    std::optional<std::int64_t> pairing;
    std::int64_t offset = static_cast<std::int64_t>(EchoAssumed) - static_cast<std::int64_t>(EchoMargin);
    EchoDelay delay;
    std::int64_t measured = 0;
    std::size_t repairs = 0;
    std::array<std::int16_t, FrameSamples> heard{};
    /// The echo state is given to speex_preprocess, for what is left of the echo.
    bool residual = false;

    State()
    {
        echo = speex_echo_state_init(Half, static_cast<int>(EchoTail));
        preprocess = speex_preprocess_state_init(Half, SampleRate);
        denoise = rnnoise_create(nullptr);
        if (echo == nullptr || preprocess == nullptr || denoise == nullptr) {
            release();
            throw std::runtime_error("The sound processing could not be created");
        }
        spx_int32_t rate = SampleRate;
        speex_echo_ctl(echo, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);
        spx_int32_t on = 1;
        spx_int32_t suppress = SpeexNoiseSuppress;
        float level = AgcLevel;
        spx_int32_t gain = AgcMaxGain;
        speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_DENOISE, &on);
        speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &suppress);
        speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC_LEVEL, &level);
        speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &gain);
    }

    ~State() { release(); }

    State(const State &) = delete;
    State &operator=(const State &) = delete;

    void release()
    {
        if (echo != nullptr) {
            speex_echo_state_destroy(echo);
        }
        if (preprocess != nullptr) {
            speex_preprocess_state_destroy(preprocess);
        }
        if (denoise != nullptr) {
            rnnoise_destroy(denoise);
        }
    }

    void applyGainControl() const
    {
        spx_int32_t agc = settings.gainControl ? 1 : 0;
        speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC, &agc);
    }

    void setResidual(bool wanted)
    {
        if (wanted != residual) {
            residual = wanted;
            speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, wanted ? echo : nullptr);
        }
    }

    /// The echo learnt forgotten: its delay changed, or the streams were paired again.
    void forgetEcho()
    {
        speex_echo_state_reset(echo);
        offset = static_cast<std::int64_t>(EchoAssumed) - static_cast<std::int64_t>(EchoMargin);
        delay.reset();
        measured = processed;
    }

    /// Pairs the newest sample of each stream, as they are now.
    void pair()
    {
        pairing = reference.count - microphone.count;
        forgetEcho();
    }

    /// Measures the delay of the echo again, if it is time and both windows are kept.
    void measure()
    {
        if (!pairing || processed - measured < static_cast<std::int64_t>(MeasureEvery)) {
            return;
        }
        const std::int64_t heardFrom = processed - static_cast<std::int64_t>(EchoMicrophoneWindow);
        const std::int64_t playedFrom = heardFrom + static_cast<std::int64_t>(EchoEarliest) + *pairing;
        const std::int64_t playedTo = playedFrom + static_cast<std::int64_t>(EchoWindow);
        if (!microphone.has(heardFrom) || !reference.has(playedFrom) || !reference.has(playedTo - 1)) {
            return;
        }
        measured = processed;
        std::vector<std::int16_t> heard(EchoMicrophoneWindow);
        std::vector<std::int16_t> played(EchoWindow);
        microphone.copy(heardFrom, heard);
        reference.copy(playedFrom, played);
        const std::optional<EchoEstimate> estimate = measureEchoDelay(played, heard);
        if (estimate && delay.update(*estimate)) {
            offset = *delay.delay() - static_cast<std::int64_t>(EchoMargin);
            speex_echo_state_reset(echo);
            logger().debug("The echo comes {} ms after the sound played (correlation {:.2f})",
                           *delay.delay() * 1000 / SampleRate, estimate->correlation);
        }
    }

    /// One half of a frame of the microphone, from the sample processed, through the chain.
    void processHalf(std::span<std::int16_t, SoundProcessing::HalfFrame> out)
    {
        std::array<std::int16_t, SoundProcessing::HalfFrame> heard{};
        microphone.copy(processed, heard);
        if (pairing) {
            std::array<std::int16_t, SoundProcessing::HalfFrame> played{};
            reference.copy(processed + *pairing - offset, played);
            speex_echo_cancellation(echo, heard.data(), played.data(), out.data());
        } else {
            std::ranges::copy(heard, out.begin());
        }
        setResidual(pairing.has_value());
        if (settings.noiseSuppression) {
            std::array<float, SoundProcessing::HalfFrame> in{};
            std::array<float, SoundProcessing::HalfFrame> clean{};
            std::ranges::transform(out, in.begin(), [](std::int16_t sample) { return float(sample); });
            rnnoise_process_frame(denoise, clean.data(), in.data());
            std::ranges::transform(clean, out.begin(), toSample);
        }
        // Nothing left to do without an echo nor the gain control: the samples go out as they are
        if (pairing || settings.gainControl) {
            speex_preprocess_run(preprocess, out.data());
        }
        processed += Half;
    }
};

SoundProcessing::SoundProcessing(const ProcessingSettings &settings) : _state(std::make_unique<State>())
{
    setSettings(settings);
}

SoundProcessing::~SoundProcessing() = default;

void SoundProcessing::setSettings(const ProcessingSettings &settings)
{
    _state->settings = settings;
    _state->applyGainControl();
}

const ProcessingSettings &SoundProcessing::settings() const
{
    return _state->settings;
}

void SoundProcessing::reset()
{
    const ProcessingSettings settings = _state->settings;
    _state = std::make_unique<State>();
    setSettings(settings);
}

void SoundProcessing::played(std::span<const std::int16_t> samples)
{
    _state->reference.push(samples);
}

void SoundProcessing::captured(std::span<const std::int16_t> samples)
{
    State &state = *_state;
    state.microphone.push(samples);
    // A backlog longer than the history: its oldest samples are lost
    const std::int64_t oldest = state.microphone.count - static_cast<std::int64_t>(HistorySamples);
    if (state.processed < oldest) {
        state.processed += ((oldest - state.processed + Half - 1) / Half) * Half;
    }
    if (state.reference.count == 0) {
        return;
    }
    if (!state.pairing) {
        state.pair();
        return;
    }
    const std::int64_t drift = state.reference.count - state.microphone.count - *state.pairing;
    if (std::abs(drift) > static_cast<std::int64_t>(MaxDrift)) {
        logger().info("The microphone and the sound played moved by {} ms: paired again",
                      drift * 1000 / SampleRate);
        state.pair();
        ++state.repairs;
    }
}

std::size_t SoundProcessing::waiting() const
{
    return static_cast<std::size_t>(_state->microphone.count - _state->processed) / FrameSamples;
}

void SoundProcessing::drop(std::size_t frames)
{
    _state->processed += static_cast<std::int64_t>(std::min(frames, waiting()) * FrameSamples);
}

bool SoundProcessing::next(std::span<std::int16_t, FrameSamples> frame)
{
    if (waiting() == 0) {
        return false;
    }
    _state->microphone.copy(_state->processed, _state->heard);
    _state->processHalf(frame.first<HalfFrame>());
    _state->processHalf(frame.last<HalfFrame>());
    _state->measure();
    return true;
}

std::span<const std::int16_t, FrameSamples> SoundProcessing::heard() const
{
    return _state->heard;
}

std::optional<std::ptrdiff_t> SoundProcessing::echoDelay() const
{
    return _state->delay.delay();
}

std::size_t SoundProcessing::repairs() const
{
    return _state->repairs;
}

} // namespace rtype::voice
