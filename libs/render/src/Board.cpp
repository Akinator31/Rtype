#include "rtype/render/Board.hpp"

#include <cmath>
#include <cstddef>

namespace rtype::render
{

namespace
{

/// How far the outer edge of the frame of boardFrame() is, as times the size of the board: beyond anything
/// that may be drawn around it, the entities coming in or going out of the game.
constexpr float FrameSpread = 20.0f;

Vector3 subtract(Vector3 a, Vector3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 add(Vector3 a, Vector3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 scale(Vector3 vector, float factor)
{
    return {vector.x * factor, vector.y * factor, vector.z * factor};
}

float dot(Vector3 a, Vector3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Vector3 cross(Vector3 a, Vector3 b)
{
    return {(a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x)};
}

float length(Vector3 vector)
{
    return std::sqrt(dot(vector, vector));
}

Vector3 normalized(Vector3 vector)
{
    const float size = length(vector);
    return size > 0.0f ? scale(vector, 1.0f / size) : vector;
}

/// A column of a matrix, without its fourth row.
Vector3 column(const Matrix &matrix, int index)
{
    const auto first = static_cast<std::size_t>(index) * 4;
    return {matrix.values[first], matrix.values[first + 1], matrix.values[first + 2]};
}

/// A matrix from its columns: x, y and z axes, then the translation.
Matrix fromColumns(Vector3 x, Vector3 y, Vector3 z, Vector3 translation)
{
    Matrix matrix;
    matrix.values = {x.x, x.y, x.z, 0.0f, y.x,           y.y,           y.z,           0.0f,
                     z.x, z.y, z.z, 0.0f, translation.x, translation.y, translation.z, 1.0f};
    return matrix;
}

/// From the world to the space of a camera, as OpenGL looks: x right, y up, -z ahead.
Matrix lookAt(const Camera &camera)
{
    const Vector3 ahead = normalized(subtract(camera.target, camera.position));
    const Vector3 right = normalized(cross(ahead, camera.up));
    const Vector3 up = cross(right, ahead);
    Matrix view;
    // Row by row: the right, the top and the back of the camera, then where the world is from it
    view.values = {right.x,
                   up.x,
                   -ahead.x,
                   0.0f,
                   right.y,
                   up.y,
                   -ahead.y,
                   0.0f,
                   right.z,
                   up.z,
                   -ahead.z,
                   0.0f,
                   -dot(right, camera.position),
                   -dot(up, camera.position),
                   dot(ahead, camera.position),
                   1.0f};
    return view;
}

/// How long the side of the board along its normal is, in the space of the eye, for a unit of its own space:
/// what turns a depth of the board into metres.
float normalLength(const Matrix &transform)
{
    const float size = length(column(transform, 2));
    return size > 0.0f ? size : 1.0f;
}

} // namespace

Matrix multiply(const Matrix &left, const Matrix &right)
{
    Matrix product;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int index = 0; index < 4; ++index) {
                sum += left.at(row, index) * right.at(index, col);
            }
            product.values[static_cast<std::size_t>((col * 4) + row)] = sum;
        }
    }
    return product;
}

Matrix invert(const Matrix &matrix)
{
    // By the cofactors, as gluInvertMatrix() does
    const std::array<float, 16> &m = matrix.values;
    std::array<float, 16> inverse{};
    inverse[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
                 m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inverse[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
                 m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inverse[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
                 m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inverse[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
                  m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inverse[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
                 m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inverse[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
                 m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inverse[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
                 m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inverse[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
                  m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inverse[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
                 m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inverse[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
                 m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inverse[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
                  m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inverse[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
                  m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inverse[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
                 m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inverse[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
                 m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inverse[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
                  m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inverse[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
                  m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float determinant = m[0] * inverse[0] + m[1] * inverse[4] + m[2] * inverse[8] + m[3] * inverse[12];
    if (determinant == 0.0f) {
        return {};
    }
    Matrix result;
    for (std::size_t index = 0; index < inverse.size(); ++index) {
        result.values[index] = inverse[index] / determinant;
    }
    return result;
}

Vector3 transformPoint(const Matrix &matrix, Vector3 point)
{
    const auto &m = matrix.values;
    return {(m[0] * point.x) + (m[4] * point.y) + (m[8] * point.z) + m[12],
            (m[1] * point.x) + (m[5] * point.y) + (m[9] * point.z) + m[13],
            (m[2] * point.x) + (m[6] * point.y) + (m[10] * point.z) + m[14]};
}

Matrix boardPlane(const BoardView &board, float height)
{
    const Rectangle &area = board.area;
    if (area.width <= 0.0f || area.height <= 0.0f) {
        return board.transform;
    }
    // Metres for a game unit, along the width of the board; the depth in the same metres
    const float metres = length(column(board.transform, 0)) / area.width;
    const float depth = metres / normalLength(board.transform);
    // To the space of the board: x from -0.5 to 0.5, y up, z towards the eye. The 2D drawings are placed
    // towards +z of the game (away from its camera), so that the later ones are behind: z is turned around.
    const Matrix toBoard =
        fromColumns({1.0f / area.width, 0.0f, 0.0f}, {0.0f, -1.0f / area.height, 0.0f}, {0.0f, 0.0f, -depth},
                    {(-area.x / area.width) - 0.5f, 0.5f + (area.y / area.height), height * depth});
    return multiply(board.transform, toBoard);
}

Matrix boardWorld(const BoardView &board, const Camera &camera)
{
    const float seenHeight = camera.fieldOfView;
    const float boardWidth = length(column(board.transform, 0));
    const float boardHeight = length(column(board.transform, 1));
    if (seenHeight <= 0.0f || boardWidth <= 0.0f || boardHeight <= 0.0f) {
        return board.transform;
    }
    // The camera sees a rectangle of the shape of the board, as it sees one of the shape of its image
    const float seenWidth = seenHeight * boardWidth / boardHeight;
    const float depth = (boardWidth / seenWidth) / normalLength(board.transform);
    // Its plane, through its target, `lift` in front of the board
    const float distance = length(subtract(camera.target, camera.position));
    const Matrix toBoard = fromColumns({1.0f / seenWidth, 0.0f, 0.0f}, {0.0f, 1.0f / seenHeight, 0.0f},
                                       {0.0f, 0.0f, depth}, {0.0f, 0.0f, (distance + board.lift) * depth});
    return multiply(board.transform, multiply(toBoard, lookAt(camera)));
}

Vector3 gameToBoard(const BoardView &board, Vector2 position, float height)
{
    return transformPoint(boardPlane(board, height), {position.x, position.y, 0.0f});
}

Vector2 boardToGame(const BoardView &board, Vector3 point)
{
    const Vector3 local = transformPoint(invert(board.transform), point);
    const Rectangle &area = board.area;
    return {area.x + ((local.x + 0.5f) * area.width), area.y + ((0.5f - local.y) * area.height)};
}

std::array<Vector3, 8> boardFrame(const BoardView &board, Vector3 eye, float nearness)
{
    const Matrix &transform = board.transform;
    const std::array<Vector3, 4> corners{
        transformPoint(transform, {-0.5f, 0.5f, 0.0f}), transformPoint(transform, {0.5f, 0.5f, 0.0f}),
        transformPoint(transform, {0.5f, -0.5f, 0.0f}), transformPoint(transform, {-0.5f, -0.5f, 0.0f})};
    const Vector3 centre = transformPoint(transform, {0.0f, 0.0f, 0.0f});
    // On its line of sight, the point is seen where it was
    const auto pull = [&](Vector3 point) { return add(eye, scale(subtract(point, eye), nearness)); };
    std::array<Vector3, 8> frame{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
        frame[corner] = pull(corners[corner]);
        frame[corner + 4] = pull(add(centre, scale(subtract(corners[corner], centre), FrameSpread)));
    }
    return frame;
}

} // namespace rtype::render
