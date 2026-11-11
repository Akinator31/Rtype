#pragma once

#include "rtype/render/Types.hpp"

#include <vector>

namespace orbit
{

/// A brick of a level: its centre, in game units, and how many hits it takes.
struct BrickSpec {
    float x = 0.0f;
    float y = 0.0f;
    int strength = 1;
};

inline constexpr int BrickColumns = 12;
inline constexpr float BrickWidth = 130.0f;
inline constexpr float BrickHeight = 38.0f;
inline constexpr float BrickGap = 12.0f;
inline constexpr int MaxBrickStrength = 3;

/// The bricks of a level: more rows and stronger bricks as the levels go, the strongest on top, and holes in
/// a diamond pattern on even levels.
[[nodiscard]] std::vector<BrickSpec> levelLayout(int level);

/// Points for a hit on a brick, the first one after the paddle.
inline constexpr int HitScore = 10;
/// The most a hit is multiplied by, after that many bricks in a row.
inline constexpr int MaxCombo = 8;

/// Points for a hit on a brick, the `combo`-th since the ball left the paddle (from 1): HitScore, then twice
/// as much for the second, three times for the third, up to MaxCombo times.
[[nodiscard]] int hitPoints(int combo);

/// The colour of a brick with this many hits left, at its top.
[[nodiscard]] rtype::render::Color brickColor(int strength);
/// The colour of a brick with this many hits left, at its bottom: darker, as if lit from the top.
[[nodiscard]] rtype::render::Color brickBottom(int strength);

} // namespace orbit
