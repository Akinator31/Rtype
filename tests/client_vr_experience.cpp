#include "Game/EntitySounds.hpp"
#include "MainMenu/VrMenu.hpp"
#include "Settings/VrSettings.hpp"
#include "Vr/Arrival.hpp"
#include "Vr/CockpitLife.hpp"
#include "Vr/VrControllers.hpp"
#include "Vr/VrLayout.hpp"
#include "Vr/VrMode.hpp"
#include "helpers/FakeHeadset.hpp"
#include "rtype/engine/Settings/Settings.hpp"
#include "rtype/game/Constants.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using Catch::Approx;
using rtype::client::CockpitFeel;
using rtype::client::CockpitLife;
using rtype::client::VrMirror;
using rtype::client::VrPreferences;

namespace
{

/// The brightness of a colour that a scaled white gives, from 0 to 1.
float level(rtype::render::Color color)
{
    return static_cast<float>(color.r) / 255.0f;
}

/// Frames of a 60th of a second, for a time in seconds.
void run(CockpitLife &life, const CockpitFeel &feel, float seconds)
{
    constexpr float Frame = 1.0f / 60.0f;
    for (float time = 0.0f; time < seconds; time += Frame) {
        life.update(Frame, feel);
    }
}

/// How many alarms sound in a time, in frames of a 60th of a second.
int alarms(CockpitLife &life, const CockpitFeel &feel, float seconds)
{
    constexpr float Frame = 1.0f / 60.0f;
    int count = 0;
    for (float time = 0.0f; time < seconds; time += Frame) {
        life.update(Frame, feel);
        count += life.takeAlarm() ? 1 : 0;
    }
    return count;
}

constexpr CockpitFeel Whole{
    .playing = true, .health = 90, .maxHealth = 90, .boss = false, .pilot = {239, 83, 80, 255}};

/// A cockpit past the start of its game.
CockpitLife started()
{
    CockpitLife life;
    run(life, Whole, rtype::client::CanopyGlowEnd + 0.5f);
    return life;
}

} // namespace

// --- The VR page ---

TEST_CASE("The VR page starts from its defaults, without settings", "[vr]")
{
    const rtype::engine::Settings settings;

    const VrPreferences preferences = rtype::client::readVrSettings(settings);

    CHECK(preferences == VrPreferences{});
    CHECK_FALSE(preferences.enabled);
    CHECK_FALSE(preferences.flatBoard);
    CHECK(preferences.mirror == VrMirror::LeftEye);
    CHECK(preferences.snapTurn == 0);
    CHECK(preferences.boardDistance == Approx(rtype::client::VrLayout{}.boardDistance));
    CHECK(preferences.boardWidth == Approx(rtype::client::VrLayout{}.boardWidth));
}

TEST_CASE("The choices of the VR page are kept and read back", "[vr]")
{
    rtype::engine::Settings settings;
    const VrPreferences chosen{.enabled = true,
                               .flatBoard = true,
                               .boardDistance = 2.6f,
                               .boardWidth = 3.3f,
                               .seatHeight = -0.05f,
                               .mirror = VrMirror::Nothing,
                               .snapTurn = 45};

    rtype::client::keepVrSettings(settings, chosen);
    const VrPreferences read = rtype::client::readVrSettings(settings);

    CHECK(read == chosen);
    // The settings of the first steps keep their meaning
    CHECK(settings.getBool(rtype::client::VrSettingsKey) == true);
    CHECK_FALSE(rtype::client::wantsReliefBoard(settings));
}

TEST_CASE("A VR choice that is not offered becomes the nearest one, a wrong one its default", "[vr]")
{
    rtype::engine::Settings settings;
    settings.setFloat(rtype::client::VrBoardDistanceKey, 2.55f);
    settings.setFloat(rtype::client::VrBoardWidthKey, 12.0f);
    settings.setFloat(rtype::client::VrSeatHeightKey, -1.0f);
    settings.setInt(rtype::client::VrSnapTurnKey, 40);
    settings.setString(rtype::client::VrMirrorKey, "the moon");

    const VrPreferences read = rtype::client::readVrSettings(settings);

    CHECK(read.boardDistance == Approx(2.6f));
    CHECK(read.boardWidth == Approx(3.3f));
    CHECK(read.seatHeight == Approx(-0.1f));
    CHECK(read.snapTurn == 45);
    CHECK(read.mirror == VrMirror::LeftEye);
}

TEST_CASE("The mirrors have names, read back", "[vr]")
{
    for (const VrMirror mirror : rtype::client::VrMirrors) {
        CHECK(rtype::client::mirrorFromName(rtype::client::mirrorName(mirror)) == mirror);
    }
    CHECK_FALSE(rtype::client::mirrorFromName("").has_value());
}

TEST_CASE("The layout follows the choices of the VR page", "[vr]")
{
    const VrPreferences preferences{.boardDistance = 3.0f, .boardWidth = 2.2f, .seatHeight = 0.1f};

    const rtype::client::VrLayout layout = rtype::client::layoutOf(preferences);

    CHECK(layout.boardDistance == Approx(3.0f));
    CHECK(layout.boardWidth == Approx(2.2f));
    CHECK(layout.seatHeight == Approx(0.1f));
    // The rest stays as it was measured
    CHECK(layout.panelDistance == Approx(rtype::client::VrLayout{}.panelDistance));
}

TEST_CASE("Every board offered stays outside the canopy, with the frame of the board in relief", "[vr]")
{
    // The nose of the canopy is 1.5 m ahead of the eyes, the frame of the board in relief at 85 % of its
    // distance (render::BoardFrameNearness): the frame must stay behind the nose
    for (const float distance : rtype::client::BoardDistances) {
        CHECK(distance * 0.85f > 1.5f);
    }
}

TEST_CASE("A higher seat draws the cockpit lower around the eyes", "[vr]")
{
    rtype::client::VrLayout high;
    high.seatHeight = 0.1f;

    const auto level = rtype::client::placeCockpit(rtype::client::seatedEyePoint({}));
    const auto raised = rtype::client::placeCockpit(rtype::client::seatedEyePoint(high));

    CHECK(raised.position.y == Approx(level.position.y - 0.1f));
    CHECK(raised.position.x == Approx(level.position.x));
    CHECK(raised.position.z == Approx(level.position.z));
}

TEST_CASE("The VR page writes lengths in metres and heights in centimetres", "[vr]")
{
    CHECK(rtype::client::metresText(2.7f) == "2.7 m");
    CHECK(rtype::client::metresText(2.45f) == "2.45 m");
    CHECK(rtype::client::metresText(3.0f) == "3.0 m");
    CHECK(rtype::client::centimetresText(0.05f) == "+5 cm");
    CHECK(rtype::client::centimetresText(0.0f) == "0 cm");
    CHECK(rtype::client::centimetresText(-0.1f) == "-10 cm");
}

// --- The headset, turned and closed ---

TEST_CASE("A turn and a close go to the headset, a close without telling a lost session", "[vr]")
{
    testing::FakeHeadsetCalls calls;
    testing::FakeHeadset *headset = nullptr;
    std::vector<bool> paces;
    int lost = 0;
    rtype::client::VrMode vr({.open =
                                  [&] {
                                      auto fake = std::make_unique<testing::FakeHeadset>(calls);
                                      headset = fake.get();
                                      return rtype::xr::OpenedHeadset{.headset = std::move(fake),
                                                                      .error = {}};
                                  },
                              .pace = [&](bool paced) { paces.push_back(paced); },
                              .lost = [&] { ++lost; },
                              .drawEye = nullptr,
                              .located = nullptr});
    vr.turn(30.0f);
    REQUIRE(calls.turns.empty());

    REQUIRE(vr.start());
    headset->currentState = rtype::xr::HeadsetState::Running;
    vr.update();
    vr.turn(-45.0f);
    CHECK(calls.turns == std::vector<float>{-45.0f});

    vr.close();
    CHECK(calls.destroyed);
    CHECK_FALSE(vr.isActive());
    CHECK(paces == std::vector<bool>{true, false});
    CHECK(lost == 0);
    // Open again from the VR page
    CHECK(vr.start());
}

// --- The snap turn ---

TEST_CASE("A flick of the right thumbstick turns the player once, by the step chosen", "[vr]")
{
    rtype::client::VrControllers controllers;
    std::array<rtype::xr::HandState, rtype::xr::HandCount> hands{};
    rtype::client::ControllerFrame frame;
    frame.elapsed = 1.0f / 60.0f;
    frame.showing = true;
    frame.snapTurn = 45;
    const auto flick = [&](float x) {
        hands.at(1).stickX = x;
        controllers.place(hands, std::nullopt);
        controllers.update(frame);
        return controllers.turnAsked();
    };

    CHECK(flick(0.0f) == 0.0f);
    CHECK(flick(0.9f) == 45.0f);
    // Held: once
    CHECK(flick(1.0f) == 0.0f);
    CHECK(flick(0.5f) == 0.0f);
    CHECK(flick(0.1f) == 0.0f);
    CHECK(flick(-0.8f) == -45.0f);
}

TEST_CASE("Without a snap turn, the right thumbstick turns nothing", "[vr]")
{
    rtype::client::VrControllers controllers;
    std::array<rtype::xr::HandState, rtype::xr::HandCount> hands{};
    hands.at(1).stickX = 1.0f;
    controllers.place(hands, std::nullopt);

    rtype::client::ControllerFrame frame;
    frame.elapsed = 1.0f / 60.0f;
    frame.showing = true;
    controllers.update(frame);

    CHECK(controllers.turnAsked() == 0.0f);
}

// --- The arrival ---

TEST_CASE("The arrival fades space in from black, then unfolds the panel", "[vr]")
{
    rtype::client::Arrival arrival;
    CHECK(arrival.brightness() == Approx(1.0f));
    CHECK_FALSE(arrival.holdsPanel());

    arrival.start();
    CHECK(arrival.brightness() == Approx(0.0f));
    CHECK(arrival.holdsPanel());

    arrival.update(rtype::client::Arrival::PanelAt / 2.0f);
    const float half = arrival.brightness();
    CHECK(half > 0.0f);
    CHECK(half < 1.0f);
    CHECK(arrival.holdsPanel());

    arrival.update(rtype::client::Arrival::PanelAt);
    CHECK_FALSE(arrival.holdsPanel());
    arrival.update(rtype::client::Arrival::FadeIn);
    CHECK(arrival.brightness() == Approx(1.0f));
}

// --- The cockpit alive ---

TEST_CASE("Out of a game, the cockpit has the lights of the artist", "[vr]")
{
    CockpitLife life;

    life.update(0.1f, {.playing = false});

    const rtype::client::CockpitLights &lights = life.lights();
    CHECK(lights.brightness == Approx(1.0f));
    CHECK(level(lights.cyan) == Approx(1.0f));
    CHECK(level(lights.orange) == Approx(1.0f));
    for (const rtype::render::Color screen : lights.screens) {
        CHECK(level(screen) == Approx(1.0f));
    }
    CHECK(level(lights.canopy) == Approx(0.0f));
    CHECK_FALSE(life.takeAlarm());
}

TEST_CASE("At the start of a game the cockpit fades in, its screens light up one after the other", "[vr]")
{
    CockpitLife life;

    life.update(1.0f / 60.0f, Whole);
    CHECK(life.lights().brightness == Approx(0.0f));
    for (const rtype::render::Color screen : life.lights().screens) {
        CHECK(level(screen) == Approx(0.0f));
    }

    // The first screen lit, the last one still dark
    run(life, Whole, rtype::client::CockpitFirstScreen + 0.35f);
    CHECK(life.lights().brightness == Approx(1.0f));
    CHECK(level(life.lights().screens.front()) == Approx(1.0f));
    CHECK(level(life.lights().screens.back()) == Approx(0.0f));

    run(life, Whole, 1.0f);
    for (const rtype::render::Color screen : life.lights().screens) {
        CHECK(level(screen) == Approx(1.0f));
    }
}

TEST_CASE("At the start of a game the canopy glows at the colour of the player, then fades", "[vr]")
{
    CockpitLife life;
    run(life, Whole, 0.4f);

    const rtype::render::Color glow = life.lights().canopy;
    // Red, the colour of slot 1, dimmed
    CHECK(glow.r > glow.g);
    CHECK(glow.r > glow.b);
    CHECK(glow.r < 239);

    run(life, Whole, rtype::client::CanopyGlowEnd);
    CHECK(level(life.lights().canopy) == Approx(0.0f));
}

TEST_CASE("A hit turns the lights of the cockpit red for a moment, and sounds the alarm once", "[vr]")
{
    CockpitLife life = started();
    CockpitFeel hit = Whole;
    hit.health = 80;

    life.update(1.0f / 60.0f, hit);

    CHECK(life.takeAlarm());
    CHECK_FALSE(life.takeAlarm());
    const rtype::render::Color orange = life.lights().orange;
    CHECK(orange.r > 200);
    CHECK(orange.g < 60);
    CHECK(level(life.lights().cyan) < 0.5f);

    CHECK(alarms(life, hit, rtype::client::HitFlash + 0.1f) == 0);
    CHECK(level(life.lights().orange) == Approx(1.0f));
    CHECK(level(life.lights().cyan) == Approx(1.0f));
}

TEST_CASE("Low on life, the cockpit pulses red and its alarm repeats", "[vr]")
{
    CockpitLife life = started();
    CockpitFeel low = Whole;
    low.health = 30;

    life.update(1.0f / 60.0f, low);
    CHECK(life.takeAlarm());
    // Then about once every LowLifeAlarm
    CHECK(alarms(life, low, 3.0f * rtype::client::LowLifeAlarm + 0.05f) == 3);
    CHECK(life.lights().orange.g < life.lights().orange.r / 4);

    // Healed: back to the lights of the artist
    run(life, Whole, rtype::client::HitFlash + 0.1f);
    CHECK(level(life.lights().orange) == Approx(1.0f));
    CHECK(alarms(life, Whole, 2.0f) == 0);
}

TEST_CASE("A boss makes the orange glows pulse, without red and without an alarm", "[vr]")
{
    CockpitLife life = started();
    CockpitFeel boss = Whole;
    boss.boss = true;

    float darkest = 1.0f;
    for (int frame = 0; frame < 120; ++frame) {
        life.update(1.0f / 60.0f, boss);
        CHECK_FALSE(life.takeAlarm());
        const rtype::render::Color orange = life.lights().orange;
        // Orange is the light of the artist dimmed: white scaled
        CHECK(orange.r == orange.g);
        darkest = std::min(darkest, level(orange));
    }
    CHECK(darkest < 0.5f);
}

TEST_CASE("The screens flicker when the ship is destroyed, stay dim, and come back with it", "[vr]")
{
    CockpitLife life = started();
    CockpitFeel dead = Whole;
    dead.health = 0;

    // Some screens lit, some not, while they flicker
    bool lit = false;
    bool dark = false;
    for (int frame = 0; frame < 60; ++frame) {
        life.update(1.0f / 60.0f, dead);
        for (const rtype::render::Color screen : life.lights().screens) {
            lit = lit || level(screen) > 0.9f;
            dark = dark || level(screen) < 0.2f;
        }
    }
    CHECK(lit);
    CHECK(dark);
    CHECK_FALSE(life.takeAlarm());

    run(life, dead, rtype::client::DeathFlicker);
    for (const rtype::render::Color screen : life.lights().screens) {
        CHECK(level(screen) == Approx(rtype::client::DeadScreens).margin(0.01));
    }

    life.update(1.0f / 60.0f, Whole);
    for (const rtype::render::Color screen : life.lights().screens) {
        CHECK(level(screen) == Approx(1.0f));
    }
}

// --- The sound ---

TEST_CASE("A sound is heard from its side of the game, never fully on one side", "[vr]")
{
    CHECK(rtype::client::soundPan(rtype::game::GameWidth / 2.0f) == Approx(0.0f).margin(1e-6));
    CHECK(rtype::client::soundPan(0.0f) == Approx(-0.6f));
    CHECK(rtype::client::soundPan(rtype::game::GameWidth) == Approx(0.6f));
}
