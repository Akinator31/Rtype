#include "Vr/VrMode.hpp"
#include "helpers/FakeHeadset.hpp"
#include "rtype/engine/Settings/Settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
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
               .lost = [this] { ++lost; }}};
};

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
