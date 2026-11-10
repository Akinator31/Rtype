#pragma once

#include "rtype/render/Types.hpp"

#include <array>

/// The board in relief in a VR headset (Renderer::beginBoard()): where the game is laid in the space of an
/// eye. Pure functions, without a window, so that the tests check them.
namespace rtype::render
{

/// How the World layer is laid on the board in an eye (Renderer::beginBoard()): the game, in game units, on
/// a rectangle of the space of the eye, in metres. The 3D models keep their depth, scaled as their width.
struct BoardView {
    /// The board: the square of side 1 around the origin, its top along +y, facing +z, moved by this
    /// transform, as Renderer::drawImageQuad() takes it. The game area is stretched on it: give it its shape.
    Matrix transform;
    /// The part of the game the board shows, in game units: the game area.
    Rectangle area;
    /// How far the 3D models come out of the board, towards the eye, in game units: the plane of their camera
    /// is laid this far in front of the board, so that they stand out of it rather than sink into it.
    float lift = 0.0f;
    /// How far in front of the board the 2D drawings made after Renderer::drawGlow() are, in game units: the
    /// damage, the bars of life, over the models, as they are drawn over them in a frame. Further than the
    /// models come out.
    float overlay = 0.0f;
    /// How bright the board is, from 0 (black) to 1: dimmed behind a panel, or by a fade.
    float brightness = 1.0f;
};

/// The product of two matrices: `right` applied first, then `left`.
[[nodiscard]] Matrix multiply(const Matrix &left, const Matrix &right);
/// The inverse of a matrix, the identity if it has none.
[[nodiscard]] Matrix invert(const Matrix &matrix);
/// A point moved by a matrix.
[[nodiscard]] Vector3 transformPoint(const Matrix &matrix, Vector3 point);

/// From game units to the space of the eye, for the 2D drawings of the World layer: the area of the board
/// stretched on its rectangle, its top left at the top left of the board, y down in the game and up on the
/// board.
/// @param height How far in front of the board, towards the eye, in game units: 0 on it.
[[nodiscard]] Matrix boardPlane(const BoardView &board, float height = 0.0f);

/// From the world of an orthographic camera to the space of the eye, for the 3D models of the World layer:
/// what the camera sees is stretched on the board, its plane (through its target) `BoardView::lift` in front
/// of it, the depth scaled as the width. A camera at rest, which sees the area as the screen does
/// (rtype::client::computeGameView()), puts a model where its 2D box would be; a camera that zooms or turns
/// moves the models on the board, never the board.
/// @param camera An orthographic camera: its field of view is the height it sees, in world units.
[[nodiscard]] Matrix boardWorld(const BoardView &board, const Camera &camera);

/// Where a point of the game is on the board, in the space of the eye.
/// @param height How far in front of the board, in game units.
[[nodiscard]] Vector3 gameToBoard(const BoardView &board, Vector2 position, float height = 0.0f);
/// The point of the game seen at a point of the space of the eye, put on the plane of the board along its
/// normal: the reverse of gameToBoard().
[[nodiscard]] Vector2 boardToGame(const BoardView &board, Vector3 point);

/// What hides what goes beyond the board, as an eye sees it (Renderer::beginBoard()): a frame around the
/// board, from its edges to far beyond them, every point pulled towards the eye, on its line of sight, to
/// `nearness` of its distance. From the eye, its inner edge is the edge of the board; it is nearer than
/// anything that comes out of the board, so that the depth test hides what is around the board.
/// @param eye Where the eye is, in its space.
/// @param nearness From 0 to 1: how far the frame is, as a part of the distance of the board.
/// @return The inner corners (top left, top right, bottom right, bottom left), then the outer ones in the
/// same order.
[[nodiscard]] std::array<Vector3, 8> boardFrame(const BoardView &board, Vector3 eye, float nearness);

/// How near the frame of boardFrame() is drawn: 15 % of the way from the board to the eye, 30 cm in front of
/// a board 2 m away, much more than the models come out of it, and farther than anything of a cockpit in
/// front of the board, which must not be hidden by it.
inline constexpr float BoardFrameNearness = 0.85f;

} // namespace rtype::render
