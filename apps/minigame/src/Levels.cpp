#include "Levels.hpp"

#include "Breaker.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>

namespace orbit
{

namespace
{

constexpr int firstRows = 4;
constexpr int maxRows = 8;
/// The top of the first row, under the score line.
constexpr float bricksTop = FieldTop + 80.0f;

constexpr std::array<rtype::render::Color, MaxBrickStrength> colors{{
    {64, 196, 255, 255}, // one hit: blue
    {255, 196, 64, 255}, // two hits: gold
    {255, 84, 112, 255}, // three hits: red
}};
/// How bright the bottom of a brick is, next to its top.
constexpr float bottomShade = 0.45f;

/// Even levels leave holes in a diamond, so that the levels do not all look alike.
bool isHole(int level, int row, int column)
{
    const int centre = BrickColumns / 2;
    return level % 2 == 0 && (std::abs(column - centre) + row) % 4 == 0;
}

} // namespace

std::vector<BrickSpec> levelLayout(int level)
{
    const int rows = std::min(firstRows + level - 1, maxRows);
    const float rowWidth = (BrickColumns * BrickWidth) + ((BrickColumns - 1) * BrickGap);
    const float left = ((ArenaWidth - rowWidth) / 2.0f) + (BrickWidth / 2.0f);
    std::vector<BrickSpec> bricks;
    for (int row = 0; row < rows; ++row) {
        // The top rows are the strongest, and every level adds strength
        const int strength = std::clamp(1 + ((rows - 1 - row + level - 1) / 3), 1, MaxBrickStrength);
        for (int column = 0; column < BrickColumns; ++column) {
            if (!isHole(level, row, column)) {
                bricks.push_back({left + (static_cast<float>(column) * (BrickWidth + BrickGap)),
                                  bricksTop + (static_cast<float>(row) * (BrickHeight + BrickGap)),
                                  strength});
            }
        }
    }
    return bricks;
}

int hitPoints(int combo)
{
    return HitScore * std::clamp(combo, 1, MaxCombo);
}

rtype::render::Color brickColor(int strength)
{
    return colors[static_cast<std::size_t>(std::clamp(strength, 1, MaxBrickStrength) - 1)];
}

rtype::render::Color brickBottom(int strength)
{
    const rtype::render::Color top = brickColor(strength);
    const auto darken = [](std::uint8_t channel) {
        return static_cast<std::uint8_t>(static_cast<float>(channel) * bottomShade);
    };
    return {darken(top.r), darken(top.g), darken(top.b), top.a};
}

} // namespace orbit
