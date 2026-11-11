#include "MainMenu/IntroSystem.hpp"

#include "MainMenu/CameraFlight.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

/// How long the camera takes to fly out of the asteroids, in seconds.
constexpr float cameraFlight = 4.0f;

/// The gamepads checked for a press.
constexpr int gamepadCount = 4;

[[nodiscard]] float mix(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

} // namespace

render::Camera computeIntroView(const Intro &intro)
{
    const float progress = intro.started ? intro.time / cameraFlight : 0.0f;
    return mixViews(intro.from, intro.to, easeInOut(progress));
}

IntroSystem::IntroSystem(ecs::ComponentCaches &caches, const engine::Scenes &scenes,
                         const input::InputSource &input)
    : _scenes(scenes), _input(input), _intros(use<Intro>(caches)), _cameras(use<engine::Camera>(caches)),
      _slides(use<IntroSlide>(caches)), _anchors(use<engine::Anchor>(caches)),
      _scales(use<engine::Scale>(caches)), _opacities(use<engine::Opacity>(caches)),
      _hidden(use<ecs::Hidden>(caches))
{
}

bool IntroSystem::isAnyInputHeld() const
{
    for (int key = static_cast<int>(input::Key::A); key <= static_cast<int>(input::Key::F12); ++key) {
        if (_input.isKeyDown(static_cast<input::Key>(key))) {
            return true;
        }
    }
    for (int button = static_cast<int>(input::MouseButton::Left);
         button <= static_cast<int>(input::MouseButton::Forward); ++button) {
        if (_input.isMouseButtonDown(static_cast<input::MouseButton>(button))) {
            return true;
        }
    }
    for (int gamepad = 0; gamepad < gamepadCount; ++gamepad) {
        for (int button = static_cast<int>(input::GamepadButton::FaceDown);
             button <= static_cast<int>(input::GamepadButton::RightThumb); ++button) {
            if (_input.isGamepadButtonDown(gamepad, static_cast<input::GamepadButton>(button))) {
                return true;
            }
        }
    }
    return false;
}

void IntroSystem::start(Intro &intro)
{
    intro.started = true;
    intro.time = 0.0f;
    for (ecs::entity_t entity = 0; entity < _slides.size(); ++entity) {
        if (_slides.has(entity) && _hidden.has(entity)) {
            _hidden.erase(entity);
        }
    }
}

void IntroSystem::placeCamera(ecs::entity_t entity, const Intro &intro)
{
    if (_cameras.has(entity)) {
        _cameras.get(entity).view = computeIntroView(intro);
    }
}

void IntroSystem::placeSlides(const Intro &intro)
{
    for (ecs::entity_t entity = 0; entity < _slides.size(); ++entity) {
        if (!_slides.has(entity) || !_anchors.has(entity)) {
            continue;
        }
        const IntroSlide &slide = _slides.get(entity);
        const float time = intro.started ? intro.time - slide.delay : 0.0f;
        const float progress = engine::ease(slide.easing, std::clamp(time / slide.duration, 0.0f, 1.0f));
        engine::Anchor &anchor = _anchors.get(entity);
        anchor.x = mix(slide.from.x, slide.to.x, progress);
        anchor.y = mix(slide.from.y, slide.to.y, progress);
        if (_scales.has(entity)) {
            _scales.get(entity).value = mix(slide.fromScale, slide.toScale, progress);
        }
        if (_opacities.has(entity)) {
            _opacities.get(entity).value = mix(slide.fromOpacity, 1.0f, std::clamp(progress, 0.0f, 1.0f));
        }
    }
}

void IntroSystem::update(float elapsed)
{
    const bool isInputHeld = isAnyInputHeld();
    const bool wasPressed = isInputHeld && !_wasInputHeld;
    _wasInputHeld = isInputHeld;

    for (ecs::entity_t entity = 0; entity < _intros.size(); ++entity) {
        if (!_intros.has(entity)) {
            continue;
        }
        Intro &intro = _intros.get(entity);
        if (intro.started) {
            intro.time += elapsed;
        } else if (wasPressed && _scenes.receivesInput(entity)) {
            start(intro);
        }
        placeCamera(entity, intro);
        placeSlides(intro);
    }
}

} // namespace rtype::client
