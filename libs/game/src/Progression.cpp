#include "rtype/game/Progression.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rtype::game
{

namespace
{

constexpr std::uint32_t FullPercent = 100;

} // namespace

std::uint32_t streakMultiplier(const Progression &progression, std::uint32_t streak)
{
    const std::uint32_t cap = std::max(progression.streakCap, FullPercent);
    if (streak <= 1) {
        return FullPercent;
    }
    const std::uint64_t bonus = static_cast<std::uint64_t>(progression.streakBonus) * (streak - 1);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(FullPercent + bonus, cap));
}

std::uint32_t applyMultiplier(std::uint32_t xp, std::uint32_t percent)
{
    const std::uint64_t scaled = static_cast<std::uint64_t>(xp) * percent / FullPercent;
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(scaled, std::numeric_limits<std::uint32_t>::max()));
}

std::uint64_t xpToNextLevel(std::uint32_t level)
{
    const double n = std::max<double>(level, 1.0);
    return static_cast<std::uint64_t>(std::llround(LevelBaseXp * n * std::sqrt(n)));
}

Level levelOf(std::uint64_t totalXp)
{
    Level result;
    result.xpIntoLevel = totalXp;
    result.xpForNext = xpToNextLevel(result.level);
    while (result.xpIntoLevel >= result.xpForNext &&
           result.level < std::numeric_limits<std::uint32_t>::max()) {
        result.xpIntoLevel -= result.xpForNext;
        ++result.level;
        result.xpForNext = xpToNextLevel(result.level);
    }
    return result;
}

} // namespace rtype::game
