#include "rtype/render/Viewport.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::render::computeVisibleArea;
using rtype::render::Rectangle;
using rtype::render::Vector2;
using rtype::render::windowToGame;

namespace
{

const Vector2 game{1920.0f, 1080.0f};

bool sameArea(Rectangle actual, Rectangle expected)
{
    return actual.x == expected.x && actual.y == expected.y && actual.width == expected.width &&
           actual.height == expected.height;
}

bool samePoint(Vector2 actual, Vector2 expected)
{
    return actual.x == expected.x && actual.y == expected.y;
}

} // namespace

TEST_CASE("A window with the game's aspect ratio shows exactly the game", "[render][viewport]")
{
    REQUIRE(sameArea(computeVisibleArea({1280.0f, 720.0f}, game), {0.0f, 0.0f, 1920.0f, 1080.0f}));
    REQUIRE(sameArea(computeVisibleArea({3840.0f, 2160.0f}, game), {0.0f, 0.0f, 1920.0f, 1080.0f}));
}

TEST_CASE("A taller window shows more above and below the game", "[render][viewport]")
{
    // 16:10 screen: 120 more game units, half above, half below
    REQUIRE(sameArea(computeVisibleArea({3840.0f, 2400.0f}, game), {0.0f, -60.0f, 1920.0f, 1200.0f}));
}

TEST_CASE("A wider window shows more on the left and right of the game", "[render][viewport]")
{
    // 21:9 screen: 640 more game units, half on each side
    REQUIRE(sameArea(computeVisibleArea({2560.0f, 1080.0f}, game), {-320.0f, 0.0f, 2560.0f, 1080.0f}));
}

TEST_CASE("An empty window shows the game area", "[render][viewport]")
{
    REQUIRE(sameArea(computeVisibleArea({0.0f, 0.0f}, game), {0.0f, 0.0f, 1920.0f, 1080.0f}));
}

TEST_CASE("Window positions are scaled to game units", "[render][viewport]")
{
    // 1280x720 window, 1920x1080 game: every window pixel is 1.5 game units
    REQUIRE(samePoint(windowToGame({0.0f, 0.0f}, {1280.0f, 720.0f}, game), {0.0f, 0.0f}));
    REQUIRE(samePoint(windowToGame({640.0f, 360.0f}, {1280.0f, 720.0f}, game), {960.0f, 540.0f}));
    REQUIRE(samePoint(windowToGame({1280.0f, 720.0f}, {1280.0f, 720.0f}, game), {1920.0f, 1080.0f}));
}

TEST_CASE("Window positions follow the visible area of a window of another shape", "[render][viewport]")
{
    // 21:9 window: the left edge of the window is 320 game units left of the game
    REQUIRE(samePoint(windowToGame({0.0f, 0.0f}, {2560.0f, 1080.0f}, game), {-320.0f, 0.0f}));
    REQUIRE(samePoint(windowToGame({320.0f, 0.0f}, {2560.0f, 1080.0f}, game), {0.0f, 0.0f}));
    REQUIRE(samePoint(windowToGame({1280.0f, 540.0f}, {2560.0f, 1080.0f}, game), {960.0f, 540.0f}));

    // 16:10 window: the bottom of the window is 60 game units below the game
    REQUIRE(samePoint(windowToGame({3840.0f, 2400.0f}, {3840.0f, 2400.0f}, game), {1920.0f, 1140.0f}));
}

TEST_CASE("An empty window gives the origin", "[render][viewport]")
{
    REQUIRE(samePoint(windowToGame({100.0f, 100.0f}, {0.0f, 0.0f}, game), {0.0f, 0.0f}));
}
