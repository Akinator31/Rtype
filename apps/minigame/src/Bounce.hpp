#pragma once

namespace orbit
{

/// A direction or a speed in the plane, in game units.
struct Vector {
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const Vector &) const = default;
};

/// The velocity after hitting a surface: mirrored on it if it was going into it, unchanged otherwise, so that
/// two bricks hit at once do not send the ball back into them.
/// @param normal From the ball to what it hits, of length 1.
[[nodiscard]] Vector reflect(Vector velocity, Vector normal);

/// The velocity of the ball leaving the paddle, upwards: straight up from its middle, more and more slanted
/// towards its edges, so that the player aims with the paddle.
/// @param offset Where the ball hits, from -1 (left edge) to 1 (right edge); clamped.
[[nodiscard]] Vector leavePaddle(float offset, float speed);

/// The speed of the ball after a brick: a little faster, up to BallMaxSpeed.
[[nodiscard]] float speedUp(float speed);

} // namespace orbit
