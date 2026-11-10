#include "Vr/VrControllers.hpp"

#include <cmath>

namespace rtype::client
{

namespace
{

constexpr auto Left = static_cast<std::size_t>(xr::Hand::Left);
constexpr auto Right = static_cast<std::size_t>(xr::Hand::Right);

std::size_t indexOf(xr::Hand hand)
{
    return static_cast<std::size_t>(hand);
}

xr::Hand other(xr::Hand hand)
{
    return hand == xr::Hand::Left ? xr::Hand::Right : xr::Hand::Left;
}

bool pulled(float value)
{
    return value >= TriggerPull;
}

} // namespace

bool VrControllers::anyButton() const
{
    for (const xr::HandState &hand : _hands) {
        if (pulled(hand.trigger) || pulled(hand.squeeze) || hand.primary || hand.secondary || hand.menu ||
            hand.stickClick || std::abs(hand.stickX) >= WheelPush || std::abs(hand.stickY) >= WheelPush) {
            return true;
        }
    }
    return false;
}

void VrControllers::choosePointing()
{
    std::array<bool, xr::HandCount> pressed{};
    for (std::size_t hand = 0; hand < _hands.size(); ++hand) {
        const bool held = pulled(_hands.at(hand).trigger);
        pressed.at(hand) = held && !_triggerHeld.at(hand);
        _triggerHeld.at(hand) = held;
    }
    // A click begins when the trigger is pulled on the panel, and lasts until it is released, on or off it
    for (std::size_t hand = 0; hand < _hands.size(); ++hand) {
        if (!_triggerHeld.at(hand)) {
            _clicking.at(hand) = false;
        } else if (pressed.at(hand)) {
            _clicking.at(hand) = _panelOpen && _hits.at(hand).has_value();
        }
    }
    for (const xr::Hand hand : {xr::Hand::Left, xr::Hand::Right}) {
        if (pressed.at(indexOf(hand)) && _hits.at(indexOf(hand))) {
            _pointing = hand;
        }
    }
    // The hand that points left the panel and holds nothing on it: the other one points, if it is on it
    if (!_hits.at(indexOf(_pointing)) && !_clicking.at(indexOf(_pointing)) &&
        _hits.at(indexOf(other(_pointing)))) {
        _pointing = other(_pointing);
    }
}

void VrControllers::updatePointer(const ControllerFrame &frame)
{
    const std::size_t pointing = indexOf(_pointing);
    const std::optional<PanelHit> &hit = _hits.at(pointing);
    input::MousePosition position = PointerOff;
    if (_panelOpen && hit) {
        position = {hit->game.x, hit->game.y};
        if (_steady && std::hypot(position.x - _steady->x, position.y - _steady->y) < PointerSteadiness) {
            position = *_steady;
        }
    } else if (_clicking.at(pointing) && _steady) {
        // A slider dragged off the panel stays where it was left
        position = *_steady;
    }
    _steady = position;

    float wheel = 0.0f;
    const float push = _hands.at(Right).stickY;
    const float direction = push > 0.0f ? 1.0f : -1.0f;
    if (_panelOpen && hit && std::abs(push) >= WheelPush) {
        // Pushed the other way: at once, as a wheel turned back
        _wheelRepeat = direction == _wheelDirection ? _wheelRepeat - frame.elapsed : 0.0f;
        _wheelDirection = direction;
        if (_wheelRepeat <= 0.0f) {
            wheel = direction;
            _wheelRepeat = WheelRepeat;
        }
    } else {
        _wheelRepeat = 0.0f;
        _wheelDirection = 0.0f;
    }
    _pointer = input::PointerState{.position = position, .held = _clicking.at(pointing), .wheel = wheel};
}

void VrControllers::updateGamepad(const ControllerFrame & /*frame*/)
{
    using input::GamepadAxis;
    using input::GamepadButton;
    const xr::HandState &left = _hands.at(Left);
    const xr::HandState &right = _hands.at(Right);
    input::GamepadState gamepad;
    // OpenXR pushes y up, a gamepad down
    gamepad.axis(GamepadAxis::LeftX) = left.stickX;
    gamepad.axis(GamepadAxis::LeftY) = -left.stickY;
    gamepad.axis(GamepadAxis::RightX) = right.stickX;
    gamepad.axis(GamepadAxis::RightY) = -right.stickY;
    // While the panel is open, the triggers click on it, and only there
    gamepad.button(GamepadButton::FaceDown) = right.primary || (!_panelOpen && pulled(right.trigger));
    gamepad.button(GamepadButton::FaceRight) = right.secondary;
    gamepad.button(GamepadButton::FaceLeft) = left.primary;
    gamepad.button(GamepadButton::FaceUp) = left.secondary;
    gamepad.button(GamepadButton::Start) = left.menu || right.menu;
    gamepad.button(GamepadButton::LeftBumper) = pulled(left.squeeze);
    gamepad.button(GamepadButton::RightBumper) = pulled(right.squeeze);
    gamepad.button(GamepadButton::LeftThumb) = left.stickClick;
    gamepad.button(GamepadButton::RightThumb) = right.stickClick;
    if (!_panelOpen) {
        gamepad.axis(GamepadAxis::LeftTrigger) = left.trigger;
        gamepad.button(GamepadButton::LeftTrigger) = pulled(left.trigger);
    }
    _gamepad = gamepad;
}

void VrControllers::update(const ControllerFrame &frame)
{
    _recenter = false;
    _turn = 0.0f;
    if (!frame.showing) {
        *this = VrControllers();
        return;
    }
    _panelOpen = frame.panelOpen;
    if (frame.gamepadUsed) {
        _putDown = true;
    } else if (anyButton()) {
        _putDown = false;
    }
    choosePointing();
    updatePointer(frame);
    updateGamepad(frame);

    if (_hands.at(Right).stickClick) {
        const float before = _recenterHeld;
        _recenterHeld += frame.elapsed;
        _recenter = before < RecenterHold && _recenterHeld >= RecenterHold;
    } else {
        _recenterHeld = 0.0f;
    }

    const float sideways = _hands.at(Right).stickX;
    if (std::abs(sideways) <= TurnRelease) {
        _turnArmed = true;
    } else if (_turnArmed && frame.snapTurn > 0 && std::abs(sideways) >= TurnPush) {
        _turnArmed = false;
        _turn = std::copysign(static_cast<float>(frame.snapTurn), sideways);
    }

    const bool onPanel = _panelOpen && _hits.at(indexOf(_pointing)).has_value();
    if (onPanel && frame.hovered && frame.hovered != _hovered) {
        _vibrations.emplace_back(_pointing, HoverTick);
    }
    _hovered = onPanel ? frame.hovered : std::nullopt;
    _onWidget = onPanel && frame.hovered.has_value();
}

void VrControllers::place(const std::array<xr::HandState, xr::HandCount> &hands,
                          const std::optional<PanelSurface> &panel)
{
    _hands = hands;
    for (std::size_t hand = 0; hand < hands.size(); ++hand) {
        const xr::HandState &state = hands.at(hand);
        const Ray ray = aimRay(state.aim);
        std::optional<PanelHit> &hit = _hits.at(hand);
        hit = state.tracked && panel ? hitPanel(ray, *panel) : std::nullopt;

        HandLook &look = _looks.at(hand);
        look = {};
        look.shown = state.tracked && !_putDown;
        if (!look.shown) {
            continue;
        }
        look.controller = controllerTransform(state.grip);
        look.ray = panel.has_value();
        look.from = ray.origin;
        const float length = hit ? hit->distance : FreeRayLength;
        look.to = {ray.origin.x + (ray.direction.x * length), ray.origin.y + (ray.direction.y * length),
                   ray.origin.z + (ray.direction.z * length)};
        look.pointing = hand == indexOf(_pointing);
        look.onWidget = look.pointing && _onWidget && hit.has_value();
        if (hit) {
            look.dot = hit->point;
        }
    }
}

bool isGamepadUsed(const input::InputSource &devices)
{
    // The gamepads raylib knows of
    constexpr int Gamepads = 4;
    constexpr float Pushed = 0.5f;
    for (int gamepad = 0; gamepad < Gamepads; ++gamepad) {
        for (std::size_t button = 0; button < input::GamepadButtonCount; ++button) {
            if (devices.isGamepadButtonDown(gamepad, static_cast<input::GamepadButton>(button))) {
                return true;
            }
        }
        for (std::size_t axis = 0; axis < input::GamepadAxisCount; ++axis) {
            if (std::abs(devices.gamepadAxis(gamepad, static_cast<input::GamepadAxis>(axis))) >= Pushed) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::pair<xr::Hand, xr::Vibration>> VrControllers::takeVibrations()
{
    return std::exchange(_vibrations, {});
}

} // namespace rtype::client
