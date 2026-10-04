#include "rtype/render/Viewport.hpp"

#include <catch2/catch_test_macros.hpp>

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
