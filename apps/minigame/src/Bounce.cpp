#include "Bounce.hpp"

#include "Breaker.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace orbit
{

namespace
{

/// The most slanted the ball leaves the paddle, from straight up, in radians: 60 degrees.
constexpr float maxPaddleAngle = std::numbers::pi_v<float> / 3.0f;

} // namespace

Vector reflect(Vector velocity, Vector normal)
{
    const float into = (velocity.x * normal.x) + (velocity.y * normal.y);
    if (into <= 0.0f) {
        return velocity;
    }
    return {velocity.x - (2.0f * into * normal.x), velocity.y - (2.0f * into * normal.y)};
}

Vector leavePaddle(float offset, float speed)
{
    const float angle = std::clamp(offset, -1.0f, 1.0f) * maxPaddleAngle;
    return {std::sin(angle) * speed, -std::cos(angle) * speed};
}

float speedUp(float speed)
{
    return std::min(speed * BallSpeedup, BallMaxSpeed);
}

} // namespace orbit
