#include "rtype/voice/Processing.hpp"

#include "rtype/log/Logger.hpp"

#include <api/audio/audio_processing.h>
#include <modules/audio_processing/include/audio_processing.h>

#include <algorithm>
#include <array>
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

/// The adaptive digital gain of AGC2: how far under full scale the voice is brought, where it starts, and the
/// most it raises, in dB.
constexpr float AgcHeadroom = 12.0f;
constexpr float AgcInitialGain = 6.0f;
constexpr float AgcMaxGain = 30.0f;

} // namespace

struct SoundProcessing::State {
    ProcessingSettings settings;
    rtc::scoped_refptr<webrtc::AudioProcessing> apm;
    webrtc::StreamConfig stream{SampleRate, 1};
    /// What was played that does not make 10 ms yet.
    std::vector<std::int16_t> reference;
    /// Something was played since the reset: the echo canceller runs.
    bool echo = false;
    /// The samples of the microphone waiting, oldest first.
    std::vector<std::int16_t> microphone;
    std::array<std::int16_t, FrameSamples> heard{};

    State() : apm(webrtc::AudioProcessingBuilder().Create())
    {
        if (!apm) {
            throw std::runtime_error("The sound processing could not be created");
        }
    }

    void apply()
    {
        webrtc::AudioProcessing::Config config;
        config.high_pass_filter.enabled = true;
        config.echo_canceller.enabled = true;
        config.noise_suppression.enabled = settings.noiseSuppression;
        config.noise_suppression.level = webrtc::AudioProcessing::Config::NoiseSuppression::kHigh;
        config.gain_controller2.enabled = settings.gainControl;
        config.gain_controller2.adaptive_digital.enabled = settings.gainControl;
        // The voice brought to about -12 dBFS, as loud as the others, its peaks under clipping, and a quiet
        // microphone raised by 30 dB at most, so that the silence does not become the noise of the room
        config.gain_controller2.adaptive_digital.headroom_db = AgcHeadroom;
        config.gain_controller2.adaptive_digital.initial_gain_db = AgcInitialGain;
        config.gain_controller2.adaptive_digital.max_gain_db = AgcMaxGain;
        apm->ApplyConfig(config);
    }

    /// One half of a frame of the microphone, through the chain.
    void processHalf(std::span<const std::int16_t, SoundProcessing::HalfFrame> in,
                     std::span<std::int16_t, SoundProcessing::HalfFrame> out)
    {
        if (!echo && !settings.noiseSuppression && !settings.gainControl) {
            std::ranges::copy(in, out.begin());
            return;
        }
        if (const int error = apm->ProcessStream(in.data(), stream, stream, out.data());
            error != webrtc::AudioProcessing::kNoError) {
            logger().warning("The sound processing failed ({}): the microphone goes out as it is", error);
            std::ranges::copy(in, out.begin());
        }
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
    _state->apply();
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
    State &state = *_state;
    state.reference.insert(state.reference.end(), samples.begin(), samples.end());
    std::array<std::int16_t, HalfFrame> unused{};
    std::size_t used = 0;
    while (state.reference.size() - used >= HalfFrame) {
        state.apm->ProcessReverseStream(state.reference.data() + used, state.stream, state.stream,
                                        unused.data());
        used += HalfFrame;
        state.echo = true;
    }
    state.reference.erase(state.reference.begin(),
                          state.reference.begin() + static_cast<std::ptrdiff_t>(used));
}

void SoundProcessing::captured(std::span<const std::int16_t> samples)
{
    std::vector<std::int16_t> &microphone = _state->microphone;
    microphone.insert(microphone.end(), samples.begin(), samples.end());
    if (microphone.size() > MaxWaiting) {
        // Whole frames only, the newest kept
        drop((microphone.size() - MaxWaiting + FrameSamples - 1) / FrameSamples);
    }
}

std::size_t SoundProcessing::waiting() const
{
    return _state->microphone.size() / FrameSamples;
}

void SoundProcessing::drop(std::size_t frames)
{
    std::vector<std::int16_t> &microphone = _state->microphone;
    const auto count = static_cast<std::ptrdiff_t>(std::min(frames, waiting()) * FrameSamples);
    microphone.erase(microphone.begin(), microphone.begin() + count);
}

bool SoundProcessing::next(std::span<std::int16_t, FrameSamples> frame)
{
    State &state = *_state;
    if (waiting() == 0) {
        return false;
    }
    std::copy_n(state.microphone.begin(), FrameSamples, state.heard.begin());
    const std::span<const std::int16_t, FrameSamples> heard(state.heard);
    state.processHalf(heard.first<HalfFrame>(), frame.first<HalfFrame>());
    state.processHalf(heard.last<HalfFrame>(), frame.last<HalfFrame>());
    state.microphone.erase(state.microphone.begin(),
                           state.microphone.begin() + static_cast<std::ptrdiff_t>(FrameSamples));
    return true;
}

std::span<const std::int16_t, FrameSamples> SoundProcessing::heard() const
{
    return _state->heard;
}

} // namespace rtype::voice
