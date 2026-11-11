// The controllers of a VR headset in the client: their rays against the curved panel, the virtual gamepad and
// the pointer they give, and the vibrations of the game. No runtime: the hands are written by the tests.

#include "FakeInput.hpp"
#include "Vr/VrControllers.hpp"
#include "Vr/VrHaptics.hpp"
#include "Vr/VrLayout.hpp"
#include "Vr/VrPointer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <numbers>
#include <optional>

using rtype::client::ControllerFrame;
using rtype::client::PanelSurface;
using rtype::client::VrControllers;
using rtype::input::GamepadAxis;
using rtype::input::GamepadButton;
using rtype::xr::Hand;
using rtype::xr::HandState;

namespace
{

constexpr rtype::render::Vector2 Game{1920.0f, 1080.0f};
constexpr float Tolerance = 1e-3f;

bool close(float actual, float expected, float tolerance = Tolerance)
{
    return std::abs(actual - expected) < tolerance;
}

/// The panel of the default layout, open.
PanelSurface openPanel()
{
    const rtype::client::VrLayout layout;
    return {.transform = rtype::client::panelTransform(layout, Game, 1.0f),
            .bend = rtype::client::panelLook(layout, 1.0f, 1.0f).bend,
            .gameSize = Game};
}

/// A quaternion turning by an angle, in radians, around an axis of length 1.
rtype::xr::Quaternion turn(float angle, rtype::xr::Vector3 axis)
{
    const float sine = std::sin(angle / 2.0f);
    return {axis.x * sine, axis.y * sine, axis.z * sine, std::cos(angle / 2.0f)};
}

/// The direction that turns -z to a point.
rtype::xr::Quaternion towards(rtype::xr::Vector3 from, rtype::render::Vector3 to)
{
    const float x = to.x - from.x;
    const float y = to.y - from.y;
    const float z = to.z - from.z;
    // Heading around y, then pitch around x
    const float heading = std::atan2(-x, -z);
    const float pitch = std::atan2(y, std::hypot(x, z));
    const rtype::xr::Quaternion yaw = turn(heading, {0.0f, 1.0f, 0.0f});
    const rtype::xr::Quaternion tilt = turn(pitch, {1.0f, 0.0f, 0.0f});
    // yaw * tilt
    return {(yaw.w * tilt.x) + (yaw.x * tilt.w) + (yaw.y * tilt.z) - (yaw.z * tilt.y),
            (yaw.w * tilt.y) - (yaw.x * tilt.z) + (yaw.y * tilt.w) + (yaw.z * tilt.x),
            (yaw.w * tilt.z) + (yaw.x * tilt.y) - (yaw.y * tilt.x) + (yaw.z * tilt.w),
            (yaw.w * tilt.w) - (yaw.x * tilt.x) - (yaw.y * tilt.y) - (yaw.z * tilt.z)};
}

/// A tracked hand at the eyes, pointing at a point of the space of the headset.
HandState pointingAt(rtype::render::Vector3 point, rtype::xr::Vector3 from = {0.0f, 0.0f, 0.0f})
{
    HandState hand;
    hand.tracked = true;
    hand.aim = {.position = from, .orientation = towards(from, point)};
    hand.grip = hand.aim;
    return hand;
}

/// A tracked hand pointing down, at nothing.
HandState pointingDown()
{
    HandState hand;
    hand.tracked = true;
    hand.aim.orientation = turn(-std::numbers::pi_v<float> / 2.0f, {1.0f, 0.0f, 0.0f});
    hand.grip = hand.aim;
    return hand;
}

/// The centre of the open panel, in the space of the headset.
rtype::render::Vector3 panelCentre()
{
    const rtype::client::VrLayout layout;
    return {0.0f, layout.panelRise, -layout.panelDistance};
}

/// A frame of the client in the headset, the panel open or not.
ControllerFrame inHeadset(bool panelOpen, float elapsed = 0.016f)
{
    return {.elapsed = elapsed, .showing = true, .panelOpen = panelOpen, .hovered = {}, .gamepadUsed = false};
}

/// Places the hands, then reads them, as the client does over two frames.
void play(VrControllers &controllers, const std::array<HandState, 2> &hands, bool panelOpen,
          std::optional<std::uint32_t> hovered = {}, float elapsed = 0.016f)
{
    controllers.place(hands, panelOpen ? std::optional(openPanel()) : std::nullopt);
    ControllerFrame frame = inHeadset(panelOpen, elapsed);
    frame.hovered = hovered;
    controllers.update(frame);
}

} // namespace

TEST_CASE("The ray of a controller goes along the -z of its aim", "[client][vr]")
{
    const rtype::client::Ray ahead =
        rtype::client::aimRay({.position = {0.1f, 0.2f, 0.3f}, .orientation = {}});
    REQUIRE(close(ahead.origin.x, 0.1f));
    REQUIRE(close(ahead.direction.z, -1.0f));

    const rtype::client::Ray right = rtype::client::aimRay(
        {.position = {}, .orientation = turn(-std::numbers::pi_v<float> / 2.0f, {0, 1, 0})});
    REQUIRE(close(right.direction.x, 1.0f));
    REQUIRE(close(right.direction.z, 0.0f));
}

TEST_CASE("A ray at the centre of the panel meets the centre of the frame", "[client][vr]")
{
    const PanelSurface panel = openPanel();
    const auto hit = rtype::client::hitPanel(rtype::client::aimRay(pointingAt(panelCentre()).aim), panel);
    REQUIRE(hit);
    REQUIRE(close(hit->game.x, Game.x / 2.0f, 0.5f));
    REQUIRE(close(hit->game.y, Game.y / 2.0f, 0.5f));
    REQUIRE(close(hit->distance, std::hypot(panelCentre().y, panelCentre().z)));
}

TEST_CASE("A ray meets the curved panel where it is drawn, in game units of the frame", "[client][vr]")
{
    const PanelSurface panel = openPanel();
    // Points of the frame, put on the panel as drawImageQuad() bends it, then pointed at from the hand
    const auto onPanel = [&](float gameX, float gameY) {
        const float x = (gameX / Game.x) - 0.5f;
        const float y = 0.5f - (gameY / Game.y);
        const float z = 4.0f * panel.bend * x * x;
        const auto &m = panel.transform.values;
        return rtype::render::Vector3{(m[0] * x) + (m[4] * y) + (m[8] * z) + m[12],
                                      (m[1] * x) + (m[5] * y) + (m[9] * z) + m[13],
                                      (m[2] * x) + (m[6] * y) + (m[10] * z) + m[14]};
    };
    for (const rtype::render::Vector2 point :
         {rtype::render::Vector2{100.0f, 100.0f}, {1800.0f, 200.0f}, {300.0f, 1000.0f}, {1700.0f, 900.0f}}) {
        const rtype::xr::Vector3 hand{0.2f, -0.3f, -0.1f};
        const auto hit = rtype::client::hitPanel(
            rtype::client::aimRay(pointingAt(onPanel(point.x, point.y), hand).aim), panel);
        REQUIRE(hit);
        REQUIRE(close(hit->game.x, point.x, 0.5f));
        REQUIRE(close(hit->game.y, point.y, 0.5f));
    }
    // The sides come towards the player: a ray to a side meets the curve before the flat panel
    PanelSurface flat = panel;
    flat.bend = 0.0f;
    const rtype::client::Ray side = rtype::client::aimRay(pointingAt({0.7f, 0.05f, -1.1f}).aim);
    const auto curved = rtype::client::hitPanel(side, panel);
    const auto straight = rtype::client::hitPanel(side, flat);
    REQUIRE(curved);
    REQUIRE(straight);
    REQUIRE(curved->distance < straight->distance);
}

TEST_CASE("A ray beside the panel, or away from it, meets nothing", "[client][vr]")
{
    const PanelSurface panel = openPanel();
    REQUIRE_FALSE(rtype::client::hitPanel(rtype::client::aimRay(pointingDown().aim), panel));
    REQUIRE_FALSE(rtype::client::hitPanel(rtype::client::aimRay(pointingAt({0.0f, 0.0f, 1.0f}).aim), panel));
    REQUIRE_FALSE(
        rtype::client::hitPanel(rtype::client::aimRay(pointingAt({1.5f, 0.05f, -1.1f}).aim), panel));
    REQUIRE_FALSE(rtype::client::hitPanel(rtype::client::aimRay(pointingAt({0.0f, 1.2f, -1.1f}).aim), panel));
}

TEST_CASE("Out of the headset the controllers give nothing", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingAt(panelCentre());
    right.trigger = 1.0f;
    right.primary = true;
    controllers.place({HandState{}, right}, openPanel());
    controllers.update(
        {.elapsed = 0.016f, .showing = false, .panelOpen = true, .hovered = {}, .gamepadUsed = false});
    REQUIRE_FALSE(controllers.pointer());
    REQUIRE_FALSE(controllers.gamepad().button(GamepadButton::FaceDown));
}

TEST_CASE("The controllers are a gamepad during a game", "[client][vr]")
{
    VrControllers controllers;
    HandState left = pointingDown();
    HandState right = pointingDown();
    left.stickX = -0.8f;
    left.stickY = 0.6f;
    left.squeeze = 1.0f;
    left.trigger = 0.7f;
    left.menu = true;
    left.primary = true;
    right.trigger = 1.0f;
    right.secondary = true;
    play(controllers, {left, right}, false);

    const rtype::input::GamepadState &gamepad = controllers.gamepad();
    REQUIRE(close(gamepad.axis(GamepadAxis::LeftX), -0.8f));
    // Up on the thumbstick is up on a gamepad, where y points down
    REQUIRE(close(gamepad.axis(GamepadAxis::LeftY), -0.6f));
    REQUIRE(gamepad.button(GamepadButton::FaceDown));
    REQUIRE(gamepad.button(GamepadButton::FaceRight));
    REQUIRE(gamepad.button(GamepadButton::FaceLeft));
    REQUIRE(gamepad.button(GamepadButton::Start));
    REQUIRE(gamepad.button(GamepadButton::LeftBumper));
    REQUIRE_FALSE(gamepad.button(GamepadButton::RightBumper));
    REQUIRE(gamepad.button(GamepadButton::LeftTrigger));
    REQUIRE(close(gamepad.axis(GamepadAxis::LeftTrigger), 0.7f));
    // The panel is folded: the pointer points at nothing
    REQUIRE(controllers.pointer());
    REQUIRE(controllers.pointer()->position.x == rtype::client::PointerOff.x);
    REQUIRE_FALSE(controllers.pointer()->held);
}

TEST_CASE("On the open panel, the ray is the pointer and the trigger its button", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingAt(panelCentre());
    play(controllers, {pointingDown(), right}, true);
    REQUIRE(controllers.pointer());
    REQUIRE(close(controllers.pointer()->position.x, Game.x / 2.0f, 0.5f));
    REQUIRE(close(controllers.pointer()->position.y, Game.y / 2.0f, 0.5f));
    REQUIRE_FALSE(controllers.pointer()->held);

    right.trigger = 1.0f;
    play(controllers, {pointingDown(), right}, true);
    REQUIRE(controllers.pointer()->held);
    // A click, not the confirm of the gamepad as well
    REQUIRE_FALSE(controllers.gamepad().button(GamepadButton::FaceDown));

    // Drawn: the ray from the hand to the dot on the panel
    const auto &look = controllers.looks()[1];
    REQUIRE(look.shown);
    REQUIRE(look.ray);
    REQUIRE(look.pointing);
    REQUIRE(look.dot);
}

TEST_CASE("A trigger pulled off the open panel does nothing", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingDown();
    right.trigger = 1.0f;
    play(controllers, {pointingDown(), right}, true);
    REQUIRE_FALSE(controllers.pointer()->held);
    REQUIRE_FALSE(controllers.gamepad().button(GamepadButton::FaceDown));
    // Brought onto the panel still pulled: no click, it began elsewhere
    right = pointingAt(panelCentre());
    right.trigger = 1.0f;
    play(controllers, {pointingDown(), right}, true);
    REQUIRE_FALSE(controllers.pointer()->held);
}

TEST_CASE("The hand that pulls its trigger on the panel points", "[client][vr]")
{
    VrControllers controllers;
    HandState left = pointingAt({-0.3f, 0.05f, -1.1f});
    HandState right = pointingAt({0.3f, 0.05f, -1.1f});
    play(controllers, {left, right}, true);
    REQUIRE(controllers.pointing() == Hand::Right);
    REQUIRE(controllers.pointer()->position.x > Game.x / 2.0f);

    left.trigger = 1.0f;
    play(controllers, {left, right}, true);
    REQUIRE(controllers.pointing() == Hand::Left);
    REQUIRE(controllers.pointer()->position.x < Game.x / 2.0f);
    REQUIRE(controllers.pointer()->held);

    // The hand that points leaves the panel, the other one is on it: it points
    left = pointingDown();
    play(controllers, {left, right}, true);
    REQUIRE(controllers.pointing() == Hand::Right);
}

TEST_CASE("A hand never quite still does not move the pointer", "[client][vr]")
{
    VrControllers controllers;
    play(controllers, {pointingDown(), pointingAt(panelCentre())}, true);
    const rtype::input::MousePosition still = controllers.pointer()->position;

    const rtype::render::Vector3 centre = panelCentre();
    play(controllers, {pointingDown(), pointingAt({centre.x + 0.0005f, centre.y, centre.z})}, true);
    REQUIRE(controllers.pointer()->position.x == still.x);

    play(controllers, {pointingDown(), pointingAt({centre.x + 0.1f, centre.y, centre.z})}, true);
    REQUIRE(controllers.pointer()->position.x > still.x + 50.0f);
}

TEST_CASE("A slider dragged off the panel stays where it was left", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingAt(panelCentre());
    right.trigger = 1.0f;
    play(controllers, {pointingDown(), right}, true);
    const rtype::input::MousePosition held = controllers.pointer()->position;

    HandState away = pointingDown();
    away.trigger = 1.0f;
    play(controllers, {pointingDown(), away}, true);
    REQUIRE(controllers.pointer()->held);
    REQUIRE(controllers.pointer()->position.x == held.x);
}

TEST_CASE("The right thumbstick scrolls the panel, again while it is held", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingAt(panelCentre());
    right.stickY = 1.0f;
    play(controllers, {pointingDown(), right}, true);
    REQUIRE(close(controllers.pointer()->wheel, 1.0f));
    play(controllers, {pointingDown(), right}, true, {}, 0.05f);
    REQUIRE(close(controllers.pointer()->wheel, 0.0f));
    play(controllers, {pointingDown(), right}, true, {}, 0.2f);
    REQUIRE(close(controllers.pointer()->wheel, 1.0f));
    right.stickY = -1.0f;
    play(controllers, {pointingDown(), right}, true);
    REQUIRE(close(controllers.pointer()->wheel, -1.0f));
}

TEST_CASE("Each widget the pointer reaches ticks in the hand", "[client][vr]")
{
    VrControllers controllers;
    const HandState right = pointingAt(panelCentre());
    play(controllers, {pointingDown(), right}, true, 7u);
    auto ticks = controllers.takeVibrations();
    REQUIRE(ticks.size() == 1);
    REQUIRE(ticks[0].first == Hand::Right);
    // Still on it: nothing more; lit while on it
    play(controllers, {pointingDown(), right}, true, 7u);
    REQUIRE(controllers.takeVibrations().empty());
    REQUIRE(controllers.looks()[1].onWidget);
    play(controllers, {pointingDown(), right}, true, 8u);
    REQUIRE(controllers.takeVibrations().size() == 1);
}

TEST_CASE("The right thumbstick held in for a second recentres, once", "[client][vr]")
{
    VrControllers controllers;
    HandState right = pointingDown();
    right.stickClick = true;
    play(controllers, {pointingDown(), right}, false, {}, 0.5f);
    REQUIRE_FALSE(controllers.recenterAsked());
    play(controllers, {pointingDown(), right}, false, {}, 0.6f);
    REQUIRE(controllers.recenterAsked());
    play(controllers, {pointingDown(), right}, false, {}, 0.6f);
    REQUIRE_FALSE(controllers.recenterAsked());
}

TEST_CASE("The controllers are hidden while a gamepad is used, and shown again when used", "[client][vr]")
{
    VrControllers controllers;
    controllers.place({pointingDown(), pointingDown()}, std::nullopt);
    controllers.update(
        {.elapsed = 0.016f, .showing = true, .panelOpen = false, .hovered = {}, .gamepadUsed = true});
    controllers.place({pointingDown(), pointingDown()}, std::nullopt);
    REQUIRE_FALSE(controllers.looks()[0].shown);

    HandState right = pointingDown();
    right.primary = true;
    play(controllers, {pointingDown(), right}, false);
    controllers.place({pointingDown(), right}, std::nullopt);
    REQUIRE(controllers.looks()[1].shown);
    // Not tracked, not drawn
    controllers.place({HandState{}, right}, std::nullopt);
    REQUIRE_FALSE(controllers.looks()[0].shown);
}

TEST_CASE("A real gamepad is told from the controllers", "[client][vr]")
{
    FakeInput devices;
    REQUIRE_FALSE(rtype::client::isGamepadUsed(devices));
    devices.axes[{0, GamepadAxis::LeftX}] = 0.2f;
    REQUIRE_FALSE(rtype::client::isGamepadUsed(devices));
    devices.axes[{1, GamepadAxis::RightTrigger}] = 0.9f;
    REQUIRE(rtype::client::isGamepadUsed(devices));
    devices.axes.clear();
    devices.buttons.insert({0, GamepadButton::Start});
    REQUIRE(rtype::client::isGamepadUsed(devices));
}

TEST_CASE("Shots tick, hits pulse, and a death rumbles", "[client][vr]")
{
    rtype::client::VrHaptics haptics;
    // A game that starts with its ship: nothing
    REQUIRE(haptics.update({.playing = true, .shots = 0, .health = 3}).empty());
    auto felt = haptics.update({.playing = true, .shots = 1, .health = 3});
    REQUIRE(felt.size() == 1);
    REQUIRE(felt[0].first == Hand::Right);
    REQUIRE(close(felt[0].second.amplitude, rtype::client::ShotTick.amplitude));

    felt = haptics.update({.playing = true, .shots = 1, .health = 2});
    REQUIRE(felt.size() == 2);
    REQUIRE(close(felt[0].second.seconds, rtype::client::HitPulse.seconds));

    felt = haptics.update({.playing = true, .shots = 1, .health = 0});
    REQUIRE(felt.size() == 2);
    REQUIRE(close(felt[1].second.seconds, rtype::client::DeathRumble.seconds));
    // The ship comes back, then the game ends: nothing
    REQUIRE(haptics.update({.playing = true, .shots = 1, .health = 3}).empty());
    REQUIRE(haptics.update({.playing = false, .shots = 0, .health = 0}).empty());
    REQUIRE(haptics.update({.playing = true, .shots = 0, .health = 3}).empty());
}
