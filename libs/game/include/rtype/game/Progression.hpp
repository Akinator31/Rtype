#pragma once

#include <cstdint>

namespace rtype::game
{

/// The experience the players earn in an endless game (Rules::endless), read from config/progression.lua. The
/// multipliers are in percent, so that every machine computes the same experience.
///
/// - A player **clears** a wave when its ship is alive as the wave ends; its **streak** is the number of
///   waves it cleared in a row, back to 0 at the first wave it does not clear.
/// - Clearing a wave gives xpPerWave times the multiplier of the new streak.
/// - A Bydo shot down gives its EnemyDefinition::xp times the multiplier of the current streak.
/// - The multiplier is 100% up to a streak of 1, then streakBonus more per wave of the streak, at most
///   streakCap.
struct Progression {
    std::uint32_t xpPerWave = 100;
    /// Percent added to the multiplier for each wave of the streak after the first.
    std::uint32_t streakBonus = 25;
    /// Highest multiplier, in percent: 100 or more.
    std::uint32_t streakCap = 300;
};

/// The multiplier of a streak, in percent.
[[nodiscard]] std::uint32_t streakMultiplier(const Progression &progression, std::uint32_t streak);

/// `xp` times a multiplier in percent, rounded down, at most the largest u32.
[[nodiscard]] std::uint32_t applyMultiplier(std::uint32_t xp, std::uint32_t percent);

/// The experience to go from level `level` to the next, from level 1: LevelBaseXp times level^1.5, rounded.
inline constexpr std::uint32_t LevelBaseXp = 500;
[[nodiscard]] std::uint64_t xpToNextLevel(std::uint32_t level);

/// Where a total of experience puts a player: its level, from 1, and how far it is into it.
struct Level {
    std::uint32_t level = 1;
    /// Experience earned since the start of the level.
    std::uint64_t xpIntoLevel = 0;
    /// xpToNextLevel(level).
    std::uint64_t xpForNext = 0;

    friend bool operator==(const Level &, const Level &) = default;
};

[[nodiscard]] Level levelOf(std::uint64_t totalXp);

} // namespace rtype::game
