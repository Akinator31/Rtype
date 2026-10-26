#include "MainMenu/IntroSystem.hpp"

#include "MainMenu/Space.hpp"

#include <algorithm>

namespace rtype::client
{

namespace
{

/// How long the camera takes to fly out of the asteroids, in seconds.
constexpr float cameraFlight = 4.0f;

/// The gamepads checked for a press.
constexpr int gamepadCount = 4;

/// Starts slowly, speeds up, then slows down until it stops: the camera neither jumps nor brakes suddenly.
[[nodiscard]] float easeInOut(float progress)
{
    return progress * progress * (3.0f - (2.0f * progress));
}

[[nodiscard]] float mix(float from, float to, float amount)
{
    return from + ((to - from) * amount);
}

[[nodiscard]] render::Vector3 mix(render::Vector3 from, render::Vector3 to, float amount)
{
    return from + ((to - from) * amount);
}

/// The view between two others: the camera moves in a straight line and turns smoothly from one direction
/// to the other.
[[nodiscard]] render::Camera mix(const render::Camera &from, const render::Camera &to, float amount)
{
    const render::Vector3 fromForward = normalize(from.target - from.position);
    const render::Vector3 toForward = normalize(to.target - to.position);
    render::Camera view = to;
    view.position = mix(from.position, to.position, amount);
    view.target = view.position + normalize(mix(fromForward, toForward, amount));
    view.up = normalize(mix(from.up, to.up, amount));
    view.fieldOfView = mix(from.fieldOfView, to.fieldOfView, amount);
    return view;
}

} // namespace

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
    if (!_cameras.has(entity)) {
        return;
    }
    const float progress = intro.started ? std::min(intro.time / cameraFlight, 1.0f) : 0.0f;
    _cameras.get(entity).view = mix(intro.from, intro.to, easeInOut(progress));
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
