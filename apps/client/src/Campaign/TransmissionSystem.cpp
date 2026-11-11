#include "Campaign/TransmissionSystem.hpp"

#include "rtype/engine/Component/Sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

namespace rtype::client
{

namespace
{

/// The time of a blink of the signal, on then off.
constexpr float blinkPeriod = TransmissionSystem::BlinkOn + TransmissionSystem::BlinkOff;
constexpr float blinksDuration = blinkPeriod * static_cast<float>(TransmissionSystem::SignalBlinks);

/// The hint to skip is dim, so that it does not draw the eye from the message.
constexpr float hintOpacity = 0.55f;
/// How fast the hint pulses once the player can go on, in radians per second.
constexpr float hintPulse = 4.0f;

/// The keys of the teletype vary around their pitch by this much, up or down.
constexpr float keyPitchSpread = 0.08f;

/// How bright the radio is now: it flickers for a few hundredths of a second, twice in a few seconds.
[[nodiscard]] float flicker(float clock)
{
    if (std::fmod(clock, 4.3f) < 0.07f) {
        return 0.45f;
    }
    if (std::fmod(clock + 1.9f, 6.1f) < 0.05f) {
        return 0.7f;
    }
    return 1.0f;
}

/// A pitch between 1 - keyPitchSpread and 1 + keyPitchSpread, different from one key to the next.
[[nodiscard]] float keyPitch(std::uint32_t key)
{
    const std::uint32_t hash = (key + 1U) * 2654435761U;
    const float amount = static_cast<float>((hash >> 16U) % 1000U) / 999.0f;
    return 1.0f - keyPitchSpread + (2.0f * keyPitchSpread * amount);
}

[[nodiscard]] bool isCharacterStart(char byte)
{
    return (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U;
}

} // namespace

std::size_t countCharacters(std::string_view text)
{
    return static_cast<std::size_t>(std::ranges::count_if(text, isCharacterStart));
}

std::string_view firstCharacters(std::string_view text, std::size_t count)
{
    std::size_t seen = 0;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (isCharacterStart(text[index])) {
            if (seen == count) {
                return text.substr(0, index);
            }
            ++seen;
        }
    }
    return text;
}

std::vector<std::string> wrapWords(const std::string &text, float width,
                                   const std::function<float(const std::string &)> &measure)
{
    std::vector<std::string> lines;
    std::string current;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find(' ', start), text.size());
        const std::string word = text.substr(start, end - start);
        start = end + 1;
        if (word.empty()) {
            continue;
        }
        const std::string longer = current.empty() ? word : current + ' ' + word;
        if (!current.empty() && measure(longer) > width) {
            lines.push_back(std::exchange(current, word));
        } else {
            current = longer;
        }
    }
    lines.push_back(std::move(current));
    return lines;
}

TransmissionSystem::TransmissionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs,
                                       const engine::Scenes &scenes, const engine::Controls &controls,
                                       const input::InputSource &input)
    : _ecs(ecs), _scenes(scenes), _controls(controls), _input(input),
      _transmissions(use<Transmission>(caches)), _texts(use<engine::Text>(caches)),
      _opacities(use<engine::Opacity>(caches)), _hidden(use<ecs::Hidden>(caches))
{
}

bool TransmissionSystem::wasPressed()
{
    const bool held =
        _input.isKeyDown(input::Key::Enter) || _input.isMouseButtonDown(input::MouseButton::Left);
    const bool pressed = held && !_wasHeld;
    _wasHeld = held;
    return pressed || _controls.wasPressed(engine::Control::FaceDown);
}

void TransmissionSystem::update(float elapsed)
{
    const bool pressed = wasPressed();
    for (ecs::entity_t entity = 0; entity < _transmissions.size(); ++entity) {
        if (!_transmissions.has(entity) || !_scenes.updates(entity)) {
            continue;
        }
        Transmission &transmission = _transmissions.get(entity);
        if (pressed && _scenes.receivesInput(entity)) {
            press(transmission);
        }
        advance(transmission, elapsed);
        show(transmission);
    }
}

void TransmissionSystem::press(Transmission &transmission)
{
    // In the order of TransmissionPhase
    static constexpr std::array<Press, TransmissionPhaseCount> presses{
        &TransmissionSystem::reveal,      &TransmissionSystem::reveal, &TransmissionSystem::reveal,
        &TransmissionSystem::skipClosing, &TransmissionSystem::finish, &TransmissionSystem::ignore,
    };
    (this->*presses[static_cast<std::size_t>(transmission.phase)])(transmission);
}

void TransmissionSystem::skipClosing(Transmission &transmission)
{
    enter(transmission, TransmissionPhase::Waiting);
}

void TransmissionSystem::finish(Transmission &transmission)
{
    enter(transmission, TransmissionPhase::Done);
    if (transmission.onEnd) {
        transmission.onEnd();
    }
}

void TransmissionSystem::ignore(Transmission & /*transmission*/) {}

void TransmissionSystem::reveal(Transmission &transmission)
{
    transmission.line = transmission.lines.size();
    transmission.typed = 0.0f;
    transmission.pause = 0.0f;
    transmission.beeps = SignalBlinks + 1;
    enter(transmission, TransmissionPhase::Closing);
    // The end shows at once, with its over and out
    transmission.time = ClosingDelay;
    close(transmission, 0.0f);
}

void TransmissionSystem::enter(Transmission &transmission, TransmissionPhase phase)
{
    transmission.phase = phase;
    transmission.time = 0.0f;
    if (phase == TransmissionPhase::Signal) {
        play(transmission.channel, transmission.sounds.statics, StaticVolume);
    }
    if (phase == TransmissionPhase::Waiting && _texts.has(transmission.hint)) {
        _texts.get(transmission.hint).text = transmission.continueHint;
    }
}

void TransmissionSystem::advance(Transmission &transmission, float elapsed)
{
    transmission.clock += elapsed;
    transmission.time += elapsed;
    // In the order of TransmissionPhase
    static constexpr std::array<Step, TransmissionPhaseCount> steps{
        &TransmissionSystem::silence, &TransmissionSystem::signal, &TransmissionSystem::type,
        &TransmissionSystem::close,   &TransmissionSystem::idle,   &TransmissionSystem::idle,
    };
    (this->*steps[static_cast<std::size_t>(transmission.phase)])(transmission, elapsed);
}

void TransmissionSystem::silence(Transmission &transmission, float /*elapsed*/)
{
    if (transmission.time >= SilenceDuration) {
        enter(transmission, TransmissionPhase::Signal);
    }
}

void TransmissionSystem::idle(Transmission & /*transmission*/, float /*elapsed*/) {}

void TransmissionSystem::signal(Transmission &transmission, float /*elapsed*/)
{
    while (transmission.beeps < SignalBlinks &&
           transmission.time >= blinkPeriod * static_cast<float>(transmission.beeps)) {
        play(transmission.signal, transmission.sounds.beep, BeepVolume);
        ++transmission.beeps;
    }
    if (transmission.beeps == SignalBlinks && transmission.time >= blinksDuration) {
        play(transmission.signal, transmission.sounds.connect, ConnectVolume);
        ++transmission.beeps;
    }
    if (transmission.time >= blinksDuration + SignalHold) {
        enter(transmission, TransmissionPhase::Typing);
    }
}

void TransmissionSystem::type(Transmission &transmission, float elapsed)
{
    if (transmission.pause > 0.0f) {
        transmission.pause -= elapsed;
        return;
    }
    if (transmission.line >= transmission.lines.size()) {
        enter(transmission, TransmissionPhase::Closing);
        return;
    }
    const TransmissionLine &line = transmission.lines[transmission.line];
    const std::size_t total = countCharacters(line.text);
    if (total == 0) {
        ++transmission.line;
        transmission.pause = BlankPause;
        return;
    }
    const auto before = static_cast<std::size_t>(transmission.typed);
    transmission.typed += line.speed * elapsed;
    const std::size_t now = std::min(static_cast<std::size_t>(transmission.typed), total);
    if (now > before) {
        const std::string_view written =
            firstCharacters(line.text, now).substr(firstCharacters(line.text, before).size());
        if (written.find_first_not_of(' ') != std::string_view::npos) {
            play(line.entity, transmission.sounds.type, TypeVolume, keyPitch(transmission.keys));
            ++transmission.keys;
        }
    }
    if (now >= total) {
        play(transmission.over, transmission.sounds.line, LineVolume);
        ++transmission.line;
        transmission.typed = 0.0f;
        transmission.pause = LinePause;
    }
}

void TransmissionSystem::close(Transmission &transmission, float /*elapsed*/)
{
    if (transmission.time >= ClosingDelay && _hidden.has(transmission.over)) {
        setHidden(transmission.over, false);
        play(transmission.over, transmission.sounds.end, EndVolume);
    }
    if (transmission.time >= ClosingDelay + WaitingDelay) {
        enter(transmission, TransmissionPhase::Waiting);
    }
}

void TransmissionSystem::show(const Transmission &transmission)
{
    const TransmissionPhase phase = transmission.phase;
    const bool started = phase != TransmissionPhase::Silence;
    const float brightness = started ? flicker(transmission.clock) : 1.0f;

    const bool blinking = phase == TransmissionPhase::Signal && transmission.time < blinksDuration;
    const bool signalOn = started && (!blinking || std::fmod(transmission.time, blinkPeriod) < BlinkOn);
    setHidden(transmission.signal, !signalOn);
    setHidden(transmission.channel, !started || blinking);
    setOpacity(transmission.signal, brightness);
    setOpacity(transmission.channel, brightness);
    setOpacity(transmission.over, brightness);

    const bool cursorOn = phase == TransmissionPhase::Typing &&
                          std::fmod(transmission.clock, CursorPeriod) < CursorPeriod / 2.0f;
    for (std::size_t index = 0; index < transmission.lines.size(); ++index) {
        const TransmissionLine &line = transmission.lines[index];
        if (!_texts.has(line.entity)) {
            continue;
        }
        std::string text;
        if (index < transmission.line) {
            text = line.text;
        } else if (index == transmission.line && phase == TransmissionPhase::Typing) {
            text = std::string(firstCharacters(line.text, static_cast<std::size_t>(transmission.typed)));
            if (cursorOn) {
                text += '_';
            }
        }
        _texts.get(line.entity).text = std::move(text);
        setOpacity(line.entity, brightness);
    }

    setHidden(transmission.hint, !started || phase == TransmissionPhase::Done);
    // Once the player can go on, the hint pulses
    const float pulse = 0.5f + (0.5f * std::cos(transmission.time * hintPulse));
    setOpacity(transmission.hint,
               phase == TransmissionPhase::Waiting ? 0.35f + (0.65f * pulse) : hintOpacity);
}

void TransmissionSystem::setHidden(ecs::entity_t entity, bool hidden)
{
    if (hidden && !_hidden.has(entity)) {
        _hidden.emplace(entity);
    } else if (!hidden && _hidden.has(entity)) {
        _hidden.erase(entity);
    }
}

void TransmissionSystem::setOpacity(ecs::entity_t entity, float opacity)
{
    if (_opacities.has(entity)) {
        _opacities.get(entity).value = opacity;
    }
}

void TransmissionSystem::play(ecs::entity_t entity, audio::SoundId sound, float volume, float pitch)
{
    _ecs.addComponent<engine::SoundEffect>(entity, sound,
                                           audio::PlaySettings{.volume = volume, .pitch = pitch});
}

} // namespace rtype::client
