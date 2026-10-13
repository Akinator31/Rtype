#include "rtype/render/Viewport.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::render::computeRenderResolution;
using rtype::render::letterbox;
using rtype::render::Rectangle;

namespace
{

bool sameArea(Rectangle actual, Rectangle expected)
{
    return actual.x == expected.x && actual.y == expected.y && actual.width == expected.width &&
           actual.height == expected.height;
}

} // namespace

TEST_CASE("A window with the game's aspect ratio is filled entirely", "[render][viewport]")
{
    REQUIRE(sameArea(letterbox({1280.0f, 720.0f}, {1920.0f, 1080.0f}), {0.0f, 0.0f, 1280.0f, 720.0f}));
}

TEST_CASE("A wider window gets black bars on the left and right", "[render][viewport]")
{
    // 21:9 screen: the 16:9 game takes the full height and is centred horizontally
    REQUIRE(sameArea(letterbox({2560.0f, 1080.0f}, {1920.0f, 1080.0f}), {320.0f, 0.0f, 1920.0f, 1080.0f}));
}

TEST_CASE("A taller window gets black bars at the top and bottom", "[render][viewport]")
{
    // 4:3 screen: the 16:9 game takes the full width and is centred vertically
    REQUIRE(sameArea(letterbox({1600.0f, 1200.0f}, {1920.0f, 1080.0f}), {0.0f, 150.0f, 1600.0f, 900.0f}));
}

TEST_CASE("The game is scaled up in a window larger than its resolution", "[render][viewport]")
{
    REQUIRE(sameArea(letterbox({3840.0f, 2160.0f}, {1920.0f, 1080.0f}), {0.0f, 0.0f, 3840.0f, 2160.0f}));
}

TEST_CASE("An empty window gives an empty area", "[render][viewport]")
{
    // A minimised window has a size of zero
    REQUIRE(sameArea(letterbox({0.0f, 0.0f}, {1920.0f, 1080.0f}), {}));
    REQUIRE(sameArea(letterbox({1280.0f, 720.0f}, {0.0f, 0.0f}), {}));
}

namespace
{

bool samePoint(rtype::render::Vector2 actual, rtype::render::Vector2 expected)
{
    return actual.x == expected.x && actual.y == expected.y;
}

} // namespace

using rtype::render::windowToGame;

TEST_CASE("Window positions are scaled to game units", "[render][viewport]")
{
    // 1280x720 window, 1920x1080 game: every window pixel is 1.5 game units
    REQUIRE(samePoint(windowToGame({0.0f, 0.0f}, {1280.0f, 720.0f}, {1920.0f, 1080.0f}), {0.0f, 0.0f}));
    REQUIRE(
        samePoint(windowToGame({640.0f, 360.0f}, {1280.0f, 720.0f}, {1920.0f, 1080.0f}), {960.0f, 540.0f}));
    REQUIRE(samePoint(windowToGame({1280.0f, 720.0f}, {1280.0f, 720.0f}, {1920.0f, 1080.0f}),
                      {1920.0f, 1080.0f}));
}

TEST_CASE("Window positions take the black bars into account", "[render][viewport]")
{
    // 21:9 window: 320 pixels of black bar on each side
    REQUIRE(samePoint(windowToGame({320.0f, 0.0f}, {2560.0f, 1080.0f}, {1920.0f, 1080.0f}), {0.0f, 0.0f}));
    REQUIRE(
        samePoint(windowToGame({1280.0f, 540.0f}, {2560.0f, 1080.0f}, {1920.0f, 1080.0f}), {960.0f, 540.0f}));

    // In the left black bar: outside of the game
    REQUIRE(windowToGame({100.0f, 540.0f}, {2560.0f, 1080.0f}, {1920.0f, 1080.0f}).x < 0.0f);
}

TEST_CASE("Converting to game units reverses the letterbox scaling", "[render][viewport]")
{
    // 4:3 window: the game area starts 150 pixels down and is scaled by 1600 / 1920
    const rtype::render::Vector2 game{1920.0f, 1080.0f};
    const rtype::render::Vector2 window{1600.0f, 1200.0f};
    const Rectangle area = letterbox(window, game);

    const rtype::render::Vector2 corner =
        windowToGame({area.x + area.width, area.y + area.height}, window, game);
    REQUIRE(corner.x == game.x);
    REQUIRE(corner.y == game.y);
}

TEST_CASE("An empty window gives the origin", "[render][viewport]")
{
    REQUIRE(samePoint(windowToGame({100.0f, 100.0f}, {0.0f, 0.0f}, {1920.0f, 1080.0f}), {0.0f, 0.0f}));
}

TEST_CASE("The game is drawn at the resolution of the area it fills", "[render][viewport]")
{
    // A 16:10 laptop screen: the image is 3840x2160 pixels, shown with bars, without being stretched
    const rtype::render::Vector2 resolution = computeRenderResolution({3840.0f, 2400.0f}, {1920.0f, 1080.0f});
    REQUIRE(resolution.x == 3840.0f);
    REQUIRE(resolution.y == 2160.0f);

    const rtype::render::Vector2 rounded = computeRenderResolution({1000.0f, 1000.0f}, {1920.0f, 1080.0f});
    REQUIRE(rounded.x == 1000.0f);
    REQUIRE(rounded.y == 563.0f);
}

TEST_CASE("A minimised window has no render resolution", "[render][viewport]")
{
    const rtype::render::Vector2 resolution = computeRenderResolution({0.0f, 0.0f}, {1920.0f, 1080.0f});
    REQUIRE(resolution.x == 0.0f);
    REQUIRE(resolution.y == 0.0f);
}
