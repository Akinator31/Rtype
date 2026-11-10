#include "rtype/render/Board.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>

using rtype::render::BoardView;
using rtype::render::Camera;
using rtype::render::Matrix;
using rtype::render::Vector2;
using rtype::render::Vector3;

namespace
{

constexpr float Tolerance = 1e-4f;

bool close(float actual, float expected, float tolerance = Tolerance)
{
    return std::abs(actual - expected) < tolerance;
}

bool samePoint(Vector3 actual, Vector3 expected)
{
    return close(actual.x, expected.x) && close(actual.y, expected.y) && close(actual.z, expected.z);
}

bool samePoint(Vector2 actual, Vector2 expected)
{
    // Game units: a thousandth of one is far below a pixel
    return close(actual.x, expected.x, 1e-3f) && close(actual.y, expected.y, 1e-3f);
}

float distance(Vector3 a, Vector3 b)
{
    return std::sqrt(((a.x - b.x) * (a.x - b.x)) + ((a.y - b.y) * (a.y - b.y)) + ((a.z - b.z) * (a.z - b.z)));
}

/// A board 3.2 m wide, 1.8 m high, 2 m ahead and 0.3 m over the eyes, tilted around x to face them.
BoardView tiltedBoard()
{
    const float tilt = std::atan2(0.3f, 2.0f);
    BoardView board;
    board.transform.values = {3.2f,
                              0.0f,
                              0.0f,
                              0.0f,
                              0.0f,
                              1.8f * std::cos(tilt),
                              1.8f * std::sin(tilt),
                              0.0f,
                              0.0f,
                              -std::sin(tilt),
                              std::cos(tilt),
                              0.0f,
                              0.0f,
                              0.3f,
                              -2.0f,
                              1.0f};
    board.area = {0.0f, 0.0f, 1920.0f, 1080.0f};
    return board;
}

/// The camera of the game at rest: it sees the area as the screen does (rtype::client::computeGameView()).
Camera restingCamera(const BoardView &board)
{
    const float x = board.area.x + (board.area.width / 2.0f);
    const float y = board.area.y + (board.area.height / 2.0f);
    return {{x, y, -1000.0f},
            {x, y, 0.0f},
            {0.0f, -1.0f, 0.0f},
            board.area.height,
            rtype::render::Projection::Orthographic};
}

/// The normal of the board, towards the eyes.
Vector3 normalOf(const BoardView &board)
{
    const auto &m = board.transform.values;
    const float length = std::sqrt((m[8] * m[8]) + (m[9] * m[9]) + (m[10] * m[10]));
    return {m[8] / length, m[9] / length, m[10] / length};
}

} // namespace

TEST_CASE("A matrix times its inverse is the identity", "[render][board]")
{
    const Matrix board = tiltedBoard().transform;
    const Matrix product = rtype::render::multiply(board, rtype::render::invert(board));
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            REQUIRE(close(product.at(row, column), row == column ? 1.0f : 0.0f));
        }
    }
    // Without an inverse, the identity
    Matrix flat;
    flat.values[0] = 0.0f;
    REQUIRE(close(rtype::render::invert(flat).at(0, 0), 1.0f));
}

TEST_CASE("The game area is stretched on the board, its top left at the top left of the board",
          "[render][board]")
{
    const BoardView board = tiltedBoard();
    const auto corner = [&](float x, float y) {
        return rtype::render::transformPoint(board.transform, {x, y, 0.0f});
    };
    REQUIRE(samePoint(rtype::render::gameToBoard(board, {0.0f, 0.0f}), corner(-0.5f, 0.5f)));
    REQUIRE(samePoint(rtype::render::gameToBoard(board, {1920.0f, 0.0f}), corner(0.5f, 0.5f)));
    REQUIRE(samePoint(rtype::render::gameToBoard(board, {1920.0f, 1080.0f}), corner(0.5f, -0.5f)));
    REQUIRE(samePoint(rtype::render::gameToBoard(board, {960.0f, 540.0f}), corner(0.0f, 0.0f)));
}

TEST_CASE("A point of the game goes on the board and back", "[render][board]")
{
    BoardView board = tiltedBoard();
    board.area = {-60.0f, 20.0f, 1920.0f, 1080.0f};
    for (const Vector2 point : {Vector2{0.0f, 0.0f}, Vector2{1234.5f, 678.9f}, Vector2{-60.0f, 1100.0f},
                                Vector2{2500.0f, -300.0f}}) {
        REQUIRE(
            samePoint(rtype::render::boardToGame(board, rtype::render::gameToBoard(board, point)), point));
        // In front of the board, it is seen on the board along its normal
        REQUIRE(samePoint(rtype::render::boardToGame(board, rtype::render::gameToBoard(board, point, 80.0f)),
                          point));
    }
}

TEST_CASE("In front of the board is towards the eyes, in the metres of its width", "[render][board]")
{
    const BoardView board = tiltedBoard();
    const Vector3 on = rtype::render::gameToBoard(board, {960.0f, 540.0f});
    const Vector3 raised = rtype::render::gameToBoard(board, {960.0f, 540.0f}, 100.0f);
    // 100 game units of a board 3.2 m wide for 1920 of them
    REQUIRE(close(distance(on, raised), 100.0f * 3.2f / 1920.0f));
    REQUIRE(distance(raised, {0.0f, 0.0f, 0.0f}) < distance(on, {0.0f, 0.0f, 0.0f}));
    const Vector3 normal = normalOf(board);
    const Vector3 along{raised.x - on.x, raised.y - on.y, raised.z - on.z};
    REQUIRE(close((along.x * normal.x) + (along.y * normal.y) + (along.z * normal.z), distance(on, raised)));
}

TEST_CASE("The camera of the game at rest puts a model where its 2D box would be", "[render][board]")
{
    const BoardView board = tiltedBoard();
    const Matrix world = rtype::render::boardWorld(board, restingCamera(board));
    for (const Vector2 point : {Vector2{0.0f, 0.0f}, Vector2{1920.0f, 1080.0f}, Vector2{300.0f, 800.0f}}) {
        REQUIRE(samePoint(rtype::render::transformPoint(world, {point.x, point.y, 0.0f}),
                          rtype::render::gameToBoard(board, point)));
    }
    // Towards the camera (-z) is towards the eyes, out of the board, at the scale of its width
    REQUIRE(samePoint(rtype::render::transformPoint(world, {960.0f, 540.0f, -50.0f}),
                      rtype::render::gameToBoard(board, {960.0f, 540.0f}, 50.0f)));
}

TEST_CASE("The models are lifted out of the board towards the eyes", "[render][board]")
{
    BoardView board = tiltedBoard();
    board.lift = 40.0f;
    const Matrix world = rtype::render::boardWorld(board, restingCamera(board));
    REQUIRE(samePoint(rtype::render::transformPoint(world, {500.0f, 200.0f, 0.0f}),
                      rtype::render::gameToBoard(board, {500.0f, 200.0f}, 40.0f)));
}

TEST_CASE("A camera that zooms moves the models on the board, never the board", "[render][board]")
{
    const BoardView board = tiltedBoard();
    Camera camera = restingCamera(board);
    // Half the height seen: twice as large, around the point it looks at
    camera.fieldOfView = 540.0f;
    camera.position = {1200.0f, 300.0f, -1000.0f};
    camera.target = {1200.0f, 300.0f, 0.0f};
    const Matrix world = rtype::render::boardWorld(board, camera);
    REQUIRE(samePoint(rtype::render::transformPoint(world, {1200.0f, 300.0f, 0.0f}),
                      rtype::render::gameToBoard(board, {960.0f, 540.0f})));
    REQUIRE(samePoint(rtype::render::transformPoint(world, {1200.0f + 480.0f, 300.0f, 0.0f}),
                      rtype::render::gameToBoard(board, {1920.0f, 540.0f})));
    // The depth is scaled as the width
    const Vector3 on = rtype::render::transformPoint(world, {1200.0f, 300.0f, 0.0f});
    const Vector3 out = rtype::render::transformPoint(world, {1200.0f, 300.0f, -10.0f});
    REQUIRE(close(distance(on, out), 2.0f * 10.0f * 3.2f / 1920.0f));
}

TEST_CASE("The frame around the board is seen along its edges, nearer than it", "[render][board]")
{
    const BoardView board = tiltedBoard();
    const Vector3 eye{0.03f, -0.02f, 0.01f};
    const auto frame = rtype::render::boardFrame(board, eye, 0.8f);
    const std::array<Vector3, 4> corners{rtype::render::gameToBoard(board, {0.0f, 0.0f}),
                                         rtype::render::gameToBoard(board, {1920.0f, 0.0f}),
                                         rtype::render::gameToBoard(board, {1920.0f, 1080.0f}),
                                         rtype::render::gameToBoard(board, {0.0f, 1080.0f})};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        // On the line of sight of the corner, a fifth of the way to the eye
        const Vector3 &inner = frame[index];
        REQUIRE(close(distance(eye, inner) + distance(inner, corners[index]), distance(eye, corners[index])));
        REQUIRE(close(distance(eye, inner), 0.8f * distance(eye, corners[index])));
        // Its outer edge, far beyond the board
        REQUIRE(distance(frame[index + 4], inner) > 10.0f);
    }
}
