#include "Vr/CockpitLook.hpp"
#include "Vr/SceneFade.hpp"
#include "Vr/VrLayout.hpp"
#include "Vr/VrMode.hpp"
#include "Vr/VrView.hpp"
#include "helpers/FakeHeadset.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <string_view>
#include <utility>
#include <vector>

using rtype::client::VrMode;
using rtype::xr::HeadsetState;
using testing::FakeHeadset;
using testing::FakeHeadsetCalls;

namespace
{

/// A VrMode on a fake headset, and what the client was told.
struct Client {
    FakeHeadsetCalls calls;
    FakeHeadset *headset = nullptr;
    bool opens = true;
    int opened = 0;
    std::vector<bool> paces;
    int lost = 0;
    std::vector<int> drawnEyes;
    /// The order of what VrMode does in a frame: 'l' located, 'e' an eye drawn.
    std::vector<char> steps;
    std::array<rtype::xr::HandState, rtype::xr::HandCount> located{};
    VrMode vr{{.open =
                   [this] {
                       ++opened;
                       if (!opens) {
                           return rtype::xr::OpenedHeadset{.headset = nullptr, .error = "no runtime"};
                       }
                       auto fake = std::make_unique<FakeHeadset>(calls);
                       headset = fake.get();
                       return rtype::xr::OpenedHeadset{.headset = std::move(fake), .error = {}};
                   },
               .pace = [this](bool paced) { paces.push_back(paced); },
               .lost = [this] { ++lost; },
               .drawEye =
                   [this](int eye, const rtype::xr::EyeView & /*view*/) {
                       drawnEyes.push_back(eye);
                       steps.push_back('e');
                   },
               .located =
                   [this](const std::array<rtype::xr::HandState, rtype::xr::HandCount> &hands) {
                       located = hands;
                       steps.push_back('l');
                   }}};
};

constexpr float Tolerance = 1e-4f;

bool close(float actual, float expected)
{
    return std::abs(actual - expected) < Tolerance;
}

bool samePoint(rtype::render::Vector3 actual, rtype::render::Vector3 expected)
{
    return close(actual.x, expected.x) && close(actual.y, expected.y) && close(actual.z, expected.z);
}

float distance(rtype::render::Vector3 a, rtype::render::Vector3 b)
{
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)) + ((a.z - b.z) * (a.z - b.z)));
}

} // namespace

TEST_CASE("VR is wanted with --xr or with the setting", "[client][vr]")
{
    rtype::engine::Settings settings;
    const std::array<std::string_view, 2> withOption{"--other", "--xr"};
    const std::array<std::string_view, 1> without{"--other"};

    REQUIRE(rtype::client::wantsVr(withOption, settings));
    REQUIRE_FALSE(rtype::client::wantsVr(without, settings));
    settings.setBool(rtype::client::VrSettingsKey, true);
    REQUIRE(rtype::client::wantsVr(without, settings));
    settings.setBool(rtype::client::VrSettingsKey, false);
    REQUIRE_FALSE(rtype::client::wantsVr({}, settings));
}

TEST_CASE("Without a headset the client stays on the screen", "[client][vr]")
{
    Client client;
    client.opens = false;

    REQUIRE_FALSE(client.vr.start());
    REQUIRE(client.vr.error() == "no runtime");
    REQUIRE_FALSE(client.vr.isActive());
    client.vr.update();
    REQUIRE(client.paces.empty());
    REQUIRE(client.lost == 0);
}

TEST_CASE("A headset is opened once", "[client][vr]")
{
    Client client;

    REQUIRE(client.vr.start());
    REQUIRE(client.vr.start());
    REQUIRE(client.opened == 1);
    REQUIRE(client.vr.isActive());
    REQUIRE(client.vr.error().empty());
}

TEST_CASE("A waiting headset draws nothing and leaves the pacing to the window", "[client][vr]")
{
    Client client;
    client.vr.start();

    client.vr.update();
    client.vr.update();

    REQUIRE(client.calls.polls == 2);
    REQUIRE(client.calls.frames == 0);
    REQUIRE(client.paces.empty());
    REQUIRE_FALSE(client.vr.isShowing());
}

TEST_CASE("A running headset gets a frame with both eyes and paces the game", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;

    client.vr.update();
    client.vr.update();

    REQUIRE(client.calls.frames == 2);
    REQUIRE(client.calls.eyes == 4);
    REQUIRE(client.calls.ended == 2);
    // Each eye drawn between its begin and its end, the left one first
    REQUIRE(client.drawnEyes == std::vector<int>{0, 1, 0, 1});
    // Told once, not every frame
    REQUIRE(client.paces == std::vector<bool>{true});
    REQUIRE(client.vr.isShowing());
}

TEST_CASE("A frame not to draw is still ended", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;
    client.headset->shouldRender = false;

    client.vr.update();

    REQUIRE(client.calls.frames == 1);
    REQUIRE(client.calls.eyes == 0);
    REQUIRE(client.drawnEyes.empty());
    REQUIRE(client.calls.ended == 1);
}

TEST_CASE("A headset that stops showing the game gives the pacing back", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;
    client.vr.update();

    client.headset->currentState = HeadsetState::Waiting;
    client.vr.update();

    REQUIRE(client.paces == std::vector<bool>{true, false});
    REQUIRE(client.calls.frames == 1);
    REQUIRE(client.vr.isActive());
}

TEST_CASE("A lost session closes the headset and the game goes on on the screen", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;
    client.vr.update();

    client.headset->currentState = HeadsetState::Lost;
    client.vr.update();

    REQUIRE(client.calls.destroyed);
    REQUIRE_FALSE(client.vr.isActive());
    REQUIRE_FALSE(client.vr.isShowing());
    REQUIRE(client.paces == std::vector<bool>{true, false});
    REQUIRE(client.lost == 1);
    // Nothing more happens, and nothing is told again
    client.vr.update();
    REQUIRE(client.lost == 1);
}

TEST_CASE("A session lost during a frame is noticed at once", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;
    client.headset->loseOnFrame = true;

    client.vr.update();

    REQUIRE(client.calls.ended == 1);
    REQUIRE(client.calls.destroyed);
    REQUIRE(client.lost == 1);
    REQUIRE(client.paces == std::vector<bool>{true, false});
}

TEST_CASE("A lost headset can be opened again", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Lost;
    client.vr.update();

    REQUIRE(client.vr.start());
    REQUIRE(client.opened == 2);
    REQUIRE(client.vr.isActive());
}

TEST_CASE("The board floats ahead of the eyes, over them, of the shape of the game, facing them",
          "[client][vr]")
{
    const rtype::client::VrLayout layout{.boardWidth = 4.0f, .boardDistance = 3.0f, .boardRise = 0.6f};
    const rtype::render::Matrix transform = rtype::client::boardTransform(layout, {1920.0f, 1080.0f});
    const auto corners = rtype::client::boardCorners(layout, {1920.0f, 1080.0f});

    // Its centre, ahead and over the eyes
    REQUIRE(close(transform.values[12], 0.0f));
    REQUIRE(close(transform.values[13], 0.6f));
    REQUIRE(close(transform.values[14], -3.0f));
    // 4 m wide, 2.25 m high: the shape of the game
    REQUIRE(close(corners[1].x - corners[0].x, 4.0f));
    REQUIRE(close(distance(corners[0], corners[3]), 2.25f));
    // Tilted to face the eyes: its normal points back to them
    const float length = std::hypot(0.6f, 3.0f);
    REQUIRE(close(transform.values[8], 0.0f));
    REQUIRE(close(transform.values[9], -0.6f / length));
    REQUIRE(close(transform.values[10], 3.0f / length));
    // Its top leans towards the eyes, its bottom away from them
    REQUIRE(corners[0].z > -3.0f);
    REQUIRE(corners[3].z < -3.0f);
}

TEST_CASE("The whole board is seen from the seat without turning the head", "[client][vr]")
{
    const rtype::client::VrLayout layout;
    // The field of view of the eyes of a Quest 2, wider towards the outside
    const rtype::xr::FieldOfView leftEye{.left = -0.942f, .right = 0.698f, .up = 0.768f, .down = -0.890f};
    const rtype::xr::FieldOfView rightEye{.left = -0.698f, .right = 0.942f, .up = 0.768f, .down = -0.890f};

    for (const rtype::render::Vector3 corner : rtype::client::boardCorners(layout, {1920.0f, 1080.0f})) {
        REQUIRE(rtype::client::isSeen(corner, leftEye));
        REQUIRE(rtype::client::isSeen(corner, rightEye));
    }
    // About 67 degrees wide
    const auto corners = rtype::client::boardCorners(layout, {1920.0f, 1080.0f});
    const float width = 2.0f * std::atan2(corners[1].x, -corners[1].z) * 180.0f / std::numbers::pi_v<float>;
    REQUIRE(width > 60.0f);
    REQUIRE(width < 75.0f);
}

TEST_CASE("The bottom of the board is seen over the dashboard", "[client][vr]")
{
    const rtype::client::VrLayout layout;
    const auto corners = rtype::client::boardCorners(layout, {1920.0f, 1080.0f});

    float highestDashboard = -90.0f;
    for (const rtype::render::Vector3 point : rtype::client::CockpitDashboardTop) {
        highestDashboard = std::max(
            highestDashboard,
            rtype::client::elevation(rtype::client::cockpitToHeadset(point, rtype::client::CockpitEyePoint)));
    }
    // Along its bottom edge, from corner to corner: its middle is the lowest seen
    for (int step = 0; step <= 10; ++step) {
        const float amount = static_cast<float>(step) / 10.0f;
        const rtype::render::Vector3 point{corners[3].x + ((corners[2].x - corners[3].x) * amount),
                                           corners[3].y, corners[3].z};
        REQUIRE(rtype::client::elevation(point) > highestDashboard);
    }
}

TEST_CASE("The board is outside the canopy, near it", "[client][vr]")
{
    const rtype::client::VrLayout layout;
    // The nose of the canopy, the farthest part of the cockpit ahead of the eyes
    const rtype::render::Vector3 nose = rtype::client::cockpitToHeadset(rtype::client::CockpitDashboardTop[0],
                                                                        rtype::client::CockpitEyePoint);

    for (const rtype::render::Vector3 corner : rtype::client::boardCorners(layout, {1920.0f, 1080.0f})) {
        REQUIRE(corner.z < nose.z);
        // Near enough to be felt close: not much farther than the canopy
        REQUIRE(corner.z > nose.z - 1.0f);
    }
}

TEST_CASE("The cockpit is placed around the eyes, its nose ahead", "[client][vr]")
{
    const rtype::render::Vector3 eye{-0.17f, 0.05f, 0.0f};

    REQUIRE(samePoint(rtype::client::cockpitToHeadset(eye, eye), {0.0f, 0.0f, 0.0f}));
    // The nose ahead, the right of the pilot on the right, up stays up
    REQUIRE(samePoint(rtype::client::cockpitToHeadset({0.83f, 0.05f, 0.0f}, eye), {0.0f, 0.0f, -1.0f}));
    REQUIRE(samePoint(rtype::client::cockpitToHeadset({-0.17f, 0.05f, 0.5f}, eye), {0.5f, 0.0f, 0.0f}));
    REQUIRE(samePoint(rtype::client::cockpitToHeadset({-0.17f, 1.05f, 0.0f}, eye), {0.0f, 1.0f, 0.0f}));

    const rtype::client::CockpitPlacement placement = rtype::client::placeCockpit(eye);
    REQUIRE(placement.rotationAngle == 90.0f);
    REQUIRE(samePoint(placement.rotationAxis, {0.0f, 1.0f, 0.0f}));
}

TEST_CASE("The transparent meshes are drawn from the farthest to the nearest", "[client][vr]")
{
    const std::vector<rtype::render::ModelMesh> meshes{
        {.name = "Cockpit_Frame", .centre = {3.0f, 0.0f, 0.0f}},
        {.name = "Cockpit_Glass", .centre = {0.2f, 0.1f, 0.0f}},
        {.name = "Cockpit_Holo", .centre = {0.66f, -0.1f, 0.0f}},
        {.name = "Cockpit_Seat", .centre = {-0.3f, -0.4f, 0.0f}},
    };

    REQUIRE(rtype::client::farToNear(meshes, rtype::client::CockpitTransparentMeshes,
                                     rtype::client::CockpitEyePoint) == std::vector<int>{2, 1});
    // Seen from the nose, the canopy is the farthest
    REQUIRE(rtype::client::farToNear(meshes, rtype::client::CockpitTransparentMeshes, {1.5f, 0.0f, 0.0f}) ==
            std::vector<int>{1, 2});
    REQUIRE(rtype::client::farToNear(meshes, {}, rtype::client::CockpitEyePoint).empty());
}

TEST_CASE(
    "Each surface of the interface has the area of its screen, the panel and the board that of the game",
    "[client][vr]")
{
    using rtype::engine::Surface;
    const rtype::engine::SurfaceAreas areas = rtype::client::surfaceAreas({1920.0f, 1080.0f});
    const auto areaOf = [&](Surface surface) { return areas[static_cast<std::size_t>(surface)]; };

    REQUIRE(close(areaOf(Surface::Panel).width, 1920.0f));
    REQUIRE(close(areaOf(Surface::Board).height, 1080.0f));
    REQUIRE(close(areaOf(Surface::HudShip).width, 480.0f));
    REQUIRE(close(areaOf(Surface::Holo).height, 330.0f));
    for (const rtype::client::CockpitScreen &screen : rtype::client::CockpitScreens) {
        const rtype::render::Rectangle area = areaOf(screen.surface);
        REQUIRE(close(area.x, 0.0f));
        REQUIRE(close(area.y, 0.0f));
        // The image has the shape of the area: nothing is stretched
        REQUIRE(std::abs((screen.resolution.x / screen.resolution.y) - (area.width / area.height)) < 0.01f);
        REQUIRE_FALSE(screen.material.empty());
    }
}

TEST_CASE("The open panel is between the dashboard and the board, seen whole, curved around the player",
          "[client][vr]")
{
    const rtype::client::VrLayout layout;
    const rtype::render::Vector2 game{1920.0f, 1080.0f};
    const rtype::xr::FieldOfView quest2{.left = -0.942f, .right = 0.698f, .up = 0.768f, .down = -0.890f};
    const auto corners = rtype::client::panelCorners(layout, game);
    const auto board = rtype::client::boardCorners(layout, game);

    for (const rtype::render::Vector3 corner : corners) {
        REQUIRE(rtype::client::isSeen(corner, quest2));
        // Nearer than the board
        REQUIRE(corner.z > board[3].z);
    }
    // 1.6 m wide, of the shape of the game, 1.1 m ahead
    REQUIRE(close(corners[1].x - corners[0].x, 1.6f));
    REQUIRE(close(distance(corners[0], corners[3]), 0.9f));
    // About 72 degrees wide: read without turning the head
    const float width = 2.0f * std::atan2(corners[1].x, -corners[1].z) * 180.0f / std::numbers::pi_v<float>;
    REQUIRE(width > 65.0f);
    REQUIRE(width < 80.0f);
    // Curved once open, over everything
    const rtype::render::ImageQuadLook open = rtype::client::panelLook(layout, 1.0f, 1.0f);
    REQUIRE(close(open.bend, layout.panelBend));
    REQUIRE(open.onTop);
    REQUIRE(open.tint.r == 255);
}

TEST_CASE("The panel unfolds from the hologram", "[client][vr]")
{
    const rtype::client::VrLayout layout;
    const rtype::render::Vector2 game{1920.0f, 1080.0f};
    // The origin of the unfolding is the hologram of the cockpit
    REQUIRE(samePoint(layout.panelOrigin, rtype::client::cockpitToHeadset(rtype::client::CockpitHoloCentre,
                                                                          rtype::client::CockpitEyePoint)));

    const rtype::render::Matrix folded = rtype::client::panelTransform(layout, game, 0.0f);
    REQUIRE(samePoint({folded.values[12], folded.values[13], folded.values[14]}, layout.panelOrigin));
    // A line of light: thin, flat
    REQUIRE(folded.values[0] < 0.2f * layout.panelWidth);
    REQUIRE(std::hypot(folded.values[5], folded.values[6]) < 0.02f);
    REQUIRE(close(rtype::client::panelLook(layout, 0.0f, 1.0f).bend, 0.0f));

    // Halfway, risen, still a line; open, of its full size
    const rtype::render::Matrix risen = rtype::client::panelTransform(layout, game, 0.4f);
    REQUIRE(close(risen.values[13], layout.panelRise));
    REQUIRE(std::hypot(risen.values[5], risen.values[6]) < 0.02f);
    const rtype::render::Matrix open = rtype::client::panelTransform(layout, game, 1.0f);
    REQUIRE(close(open.values[0], layout.panelWidth));
    REQUIRE(close(open.values[14], -layout.panelDistance));
}

TEST_CASE("The board dims behind the panel during a game, and with the fade between two scenes",
          "[client][vr]")
{
    using rtype::client::boardBrightness;
    REQUIRE(close(boardBrightness(0.0f, 1.0f, true), 1.0f));
    REQUIRE(close(boardBrightness(1.0f, 1.0f, true), rtype::client::BoardBehindPanel));
    REQUIRE(close(boardBrightness(0.0f, 0.0f, true), 0.0f));
    REQUIRE(boardBrightness(0.5f, 1.0f, true) < 1.0f);
    REQUIRE(boardBrightness(0.5f, 1.0f, true) > rtype::client::BoardBehindPanel);
    // In the menus, the world of the scene stays as bright as on the screen, behind the open panel
    REQUIRE(close(boardBrightness(1.0f, 1.0f, false), 1.0f));
    REQUIRE(close(boardBrightness(1.0f, 0.5f, false), 0.5f));
}

TEST_CASE("A switch of scenes fades out, keeping the images, then in", "[client][vr]")
{
    rtype::client::SceneFade fade;
    REQUIRE(close(fade.brightness(), 1.0f));
    REQUIRE_FALSE(fade.holding());

    fade.start();
    REQUIRE(close(fade.brightness(), 1.0f));
    REQUIRE(fade.holding());
    fade.update(0.05f);
    REQUIRE(close(fade.brightness(), 0.5f));
    REQUIRE(fade.holding());
    // Black between the two scenes, then the new one is drawn
    fade.update(0.05f);
    REQUIRE(close(fade.brightness(), 0.0f));
    REQUIRE_FALSE(fade.holding());
    fade.update(0.05f);
    REQUIRE(close(fade.brightness(), 0.5f));
    fade.update(1.0f);
    REQUIRE(close(fade.brightness(), 1.0f));
    REQUIRE_FALSE(fade.holding());
}

TEST_CASE("Recentring is asked of the open headset only", "[client][vr]")
{
    Client client;
    client.vr.recenter();
    REQUIRE(client.opened == 0);

    client.vr.start();
    client.vr.recenter();
    REQUIRE(client.calls.recentred == 1);
}

TEST_CASE("What an eye sees is given to the renderer as the headset gives it", "[client][vr]")
{
    rtype::xr::EyeView eye;
    eye.view.values[12] = 0.032f;
    eye.projection.values[0] = 1.5f;
    eye.position = {-0.032f, 0.1f, 0.2f};
    eye.width = 1832;
    eye.height = 1920;

    const rtype::render::EyeView view = rtype::client::toRenderEye(eye);

    REQUIRE(view.view.values == eye.view.values);
    REQUIRE(view.projection.values == eye.projection.values);
    REQUIRE(view.position.x == -0.032f);
    REQUIRE(view.position.y == 0.1f);
    REQUIRE(view.position.z == 0.2f);
    REQUIRE(view.width == 1832);
    REQUIRE(view.height == 1920);
}

TEST_CASE("The controllers are located before the eyes are drawn", "[client][vr]")
{
    Client client;
    client.vr.start();
    client.headset->currentState = HeadsetState::Running;
    client.headset->hands[1].tracked = true;
    client.headset->hands[1].trigger = 0.8f;
    client.vr.update();
    REQUIRE(client.steps == std::vector<char>{'l', 'e', 'e'});
    REQUIRE(client.located[1].tracked);
    REQUIRE(client.vr.hands()[1].trigger == 0.8f);
    // Located even for a frame that is not drawn: the hands follow the frames of the headset
    client.headset->shouldRender = false;
    client.vr.update();
    REQUIRE(client.steps.back() == 'l');
}

TEST_CASE("A controller vibrates only while the headset shows the game", "[client][vr]")
{
    Client client;
    client.vr.vibrate(rtype::xr::Hand::Left, {});
    REQUIRE(client.vr.hands()[0].trigger == 0.0f);
    client.vr.start();
    client.vr.update();
    client.vr.vibrate(rtype::xr::Hand::Left, {});
    REQUIRE(client.headset->vibrations.empty());

    client.headset->currentState = HeadsetState::Running;
    client.vr.update();
    client.vr.vibrate(rtype::xr::Hand::Right, {.amplitude = 0.7f, .seconds = 0.1f, .frequency = 0.0f});
    REQUIRE(client.headset->vibrations.size() == 1);
    REQUIRE(client.headset->vibrations[0].first == rtype::xr::Hand::Right);
    REQUIRE(client.headset->vibrations[0].second.amplitude == 0.7f);
}
