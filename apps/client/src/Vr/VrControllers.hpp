#pragma once

#include "Vr/VrPointer.hpp"
#include "rtype/input/VirtualInput.hpp"
#include "rtype/render/Types.hpp"
#include "rtype/xr/Headset.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace rtype::client
{

/// What is drawn of a hand in the headset (VrView::drawEye()).
struct HandLook {
    /// The controller is drawn: tracked, and the player does not play with a gamepad.
    bool shown = false;
    /// The transform of the box of the controller (controllerTransform()).
    render::Matrix controller;
    /// Its ray is drawn, from `from` to `to`: the panel is open.
    bool ray = false;
    render::Vector3 from;
    render::Vector3 to;
    /// It is the hand that points: its ray is brighter, and brighter still on a widget.
    bool pointing = false;
    bool onWidget = false;
    /// Where its ray meets the panel, if it does.
    std::optional<render::Vector3> dot;
};

/// What the client gives the controllers each frame (VrControllers::update()).
struct ControllerFrame {
    /// Time since the previous frame, in seconds.
    float elapsed = 0.0f;
    /// The headset shows the game: its controllers replace the mouse. Off, they give nothing.
    bool showing = false;
    /// The panel of the menus is open: the controllers point at it, their triggers click.
    bool panelOpen = false;
    /// The widget the pointer was over at the last update of the interface (engine::InterfaceSystem), if
    /// any, as a number that changes when it is another one.
    std::optional<std::uint32_t> hovered;
    /// A real gamepad was used this frame: the controllers are put down, and no longer drawn.
    bool gamepadUsed = false;
    /// How far a flick of the right thumbstick to a side turns the player, in degrees (the VR page); 0 for
    /// no snap turn.
    int snapTurn = 0;
};

/// The controllers of the headset as a gamepad and a pointer (docs/architecture/vr.md#the-controllers):
///
/// - **The virtual gamepad**, gamepad 0 of input::VirtualInput: the left thumbstick moves (the left stick),
///   the right trigger and A fire (FaceDown), B goes back (FaceRight), X and Y are FaceLeft and FaceUp, the
///   grips aim up and down (the bumpers), the left trigger is LT (push to talk), menu is Start (the pause),
///   the right thumbstick is the right stick, and the thumbsticks pushed in are LeftThumb and RightThumb.
///   The bindings of the player apply, as for a real gamepad.
/// - **The pointer**, while the panel is open: the ray of the hand that points meets the panel, where the
///   pointer is, in game units of the panel; its trigger is the left button, the right thumbstick the
///   wheel. The hand that points is the one whose trigger was pulled last on the panel. While the panel is
///   open, the triggers only click: pulled off the panel, they do nothing, instead of confirming a widget
///   the player does not look at.
/// - **Recentring**: the right thumbstick held in for a second.
/// - **The snap turn**, if chosen: the right thumbstick flicked to a side turns the player by a step.
///
/// Each frame, update() before the engine reads the input, then place() once the headset located the
/// controllers for the frame drawn.
class VrControllers
{
    std::array<xr::HandState, xr::HandCount> _hands{};
    std::array<std::optional<PanelHit>, xr::HandCount> _hits{};
    std::array<HandLook, xr::HandCount> _looks{};
    xr::Hand _pointing = xr::Hand::Right;
    /// The trigger of each hand is held, and whether its pull began on the panel: a click held.
    std::array<bool, xr::HandCount> _triggerHeld{};
    std::array<bool, xr::HandCount> _clicking{};
    input::GamepadState _gamepad;
    std::optional<input::PointerState> _pointer;
    /// The last position of the pointer given: a ray that shakes a little does not move it.
    std::optional<input::MousePosition> _steady;
    float _wheelRepeat = 0.0f;
    /// Which way the right thumbstick scrolls: 1 up, -1 down, 0 not pushed.
    float _wheelDirection = 0.0f;
    float _recenterHeld = 0.0f;
    bool _recenter = false;
    /// The right thumbstick came back to the middle since the last snap turn: it can turn again.
    bool _turnArmed = true;
    float _turn = 0.0f;
    std::optional<std::uint32_t> _hovered;
    bool _onWidget = false;
    /// The player plays with a real gamepad: the controllers are hidden until one of their buttons is used.
    bool _putDown = false;
    bool _panelOpen = false;
    std::vector<std::pair<xr::Hand, xr::Vibration>> _vibrations;

    /// Chooses the hand that points: the one whose trigger was just pulled on the panel, or the other one if
    /// the hand that points has left it.
    void choosePointing();
    void updatePointer(const ControllerFrame &frame);
    void updateGamepad(const ControllerFrame &frame);
    [[nodiscard]] bool anyButton() const;

public:
    /// Reads what the controllers gave at the last place(): the gamepad, the pointer, a recentre, the ticks
    /// of a widget reached.
    void update(const ControllerFrame &frame);
    /// Takes where the controllers are for the frame drawn, and where their rays meet the panel, if it is
    /// open (`panel`): what is drawn of them, and where the pointer goes at the next update().
    void place(const std::array<xr::HandState, xr::HandCount> &hands,
               const std::optional<PanelSurface> &panel);

    /// The virtual gamepad, for input::VirtualInput::setGamepad().
    [[nodiscard]] const input::GamepadState &gamepad() const { return _gamepad; }
    /// The pointer, for input::VirtualInput::setPointer(): while the headset shows the game, always one, off
    /// the panel when no ray meets it, so that the mouse of the window, which shows the mirror, points at
    /// nothing.
    [[nodiscard]] const std::optional<input::PointerState> &pointer() const { return _pointer; }
    /// The player asked for a recentre at the last update().
    [[nodiscard]] bool recenterAsked() const { return _recenter; }
    /// How far the player asked to turn at the last update(), in degrees, positive to the right: 0 but on
    /// the frame of a flick.
    [[nodiscard]] float turnAsked() const { return _turn; }
    /// What is drawn of each hand.
    [[nodiscard]] const std::array<HandLook, xr::HandCount> &looks() const { return _looks; }
    /// The hand that points at the panel.
    [[nodiscard]] xr::Hand pointing() const { return _pointing; }
    /// The vibrations asked since the last call: a tick on each widget the pointer reaches.
    [[nodiscard]] std::vector<std::pair<xr::Hand, xr::Vibration>> takeVibrations();
};

/// Whether a real gamepad is used: one of its buttons held, or one of its sticks or triggers pushed. The
/// controllers of the headset are then hidden (ControllerFrame::gamepadUsed).
/// @param devices The real devices, without the controllers over them.
[[nodiscard]] bool isGamepadUsed(const input::InputSource &devices);

/// Where the pointer is when no ray meets the panel: far from every widget.
inline constexpr input::MousePosition PointerOff{-10000.0f, -10000.0f};
/// How far the pointer must move, in game units, to be moved: a hand is never quite still.
inline constexpr float PointerSteadiness = 3.0f;
/// How far a trigger is pulled to be held, from 0 to 1, and the grip.
inline constexpr float TriggerPull = 0.5f;
/// How far the right thumbstick is pushed to scroll, and how often it scrolls again while held, in seconds.
inline constexpr float WheelPush = 0.6f;
inline constexpr float WheelRepeat = 0.15f;
/// How far the right thumbstick is pushed to a side to snap turn, and how near the middle it comes back to
/// turn again.
inline constexpr float TurnPush = 0.7f;
inline constexpr float TurnRelease = 0.3f;
/// How long the right thumbstick is held in to recentre, in seconds.
inline constexpr float RecenterHold = 1.0f;
/// The tick of a widget reached by the pointer.
inline constexpr xr::Vibration HoverTick{.amplitude = 0.15f, .seconds = 0.01f, .frequency = 0.0f};

} // namespace rtype::client
