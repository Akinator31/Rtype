#pragma once

#include <cstdint>

/// Orbit Breaker: a brick breaker built on the engine of R-Type alone (ECS, physics, rendering, input, audio,
/// scenes), to show that it runs another game than R-Type.
namespace orbit
{

/// The game area, in game units: the size the renderer scales to the window.
inline constexpr float ArenaWidth = 1920.0f;
inline constexpr float ArenaHeight = 1080.0f;
inline constexpr float WallThickness = 40.0f;
/// The top of the playing field, under the score line.
inline constexpr float FieldTop = 110.0f;

inline constexpr float PaddleWidth = 220.0f;
inline constexpr float PaddleHeight = 22.0f;
inline constexpr float PaddleY = 1000.0f;
inline constexpr float PaddleSpeed = 1100.0f;

inline constexpr float BallSize = 22.0f;
inline constexpr float BallStartSpeed = 650.0f;
inline constexpr float BallMaxSpeed = 1250.0f;
/// How much faster the ball goes after each brick broken.
inline constexpr float BallSpeedup = 1.03f;

inline constexpr int StartingLives = 3;

/// The racket of the player, moved left and right along the bottom of the arena.
struct Paddle {};

/// The ball. Until it is launched, it rides on the paddle.
struct Ball {
    bool launched = false;
    float speed = BallStartSpeed;
};

/// A brick, broken once its strength is down to 0.
struct Brick {
    int strength = 1;
};

/// One of the lives drawn in the corner of the arena, from 0: lit while the player still has it.
struct LifeIcon {
    int index = 0;
};

/// What collides with what: the ball meets everything, the paddle only the walls.
enum Layer : std::uint32_t {
    WallLayer = 1U << 0U,
    BrickLayer = 1U << 1U,
    PaddleLayer = 1U << 2U,
    BallLayer = 1U << 3U,
};

/// The state of a game, shared by its systems and its scenes.
struct Match {
    int score = 0;
    int lives = StartingLives;
    int level = 1;
    /// Bricks hit since the ball last left the paddle: each one is worth more (see hitPoints()).
    int combo = 0;
    /// The ball rides on the paddle, waiting to be launched.
    bool ballWaiting = true;
    /// The best score, kept in the settings.
    int best = 0;
};

} // namespace orbit
