#pragma once

#include "rtype/database/Database.hpp"

#include <cstdint>

/// What the master keeps of the progression of the accounts in its database (table `account_progress`,
/// docs/architecture/accounts.md#progression): the experience earned online, and a few records. Plain
/// functions on a Database, as Accounts/AccountStore.hpp: the master runs them on its database thread. The
/// level is not kept: rtype::game::levelOf() finds it from the experience.
namespace rtype::server
{

/// The progression of an account: all 0 until its first game.
struct AccountProgress {
    std::uint64_t xp = 0;
    std::uint32_t gamesPlayed = 0;
    std::uint32_t totalKills = 0;
    /// The most waves cleared in a game, and the longest streak.
    std::uint16_t bestWaves = 0;
    std::uint16_t bestStreak = 0;

    friend bool operator==(const AccountProgress &, const AccountProgress &) = default;
};

/// What a player did in a game, from the GAME_OVER of its instance.
struct GameResult {
    std::uint32_t xp = 0;
    std::uint32_t kills = 0;
    std::uint16_t wavesCleared = 0;
    std::uint16_t bestStreak = 0;
};

/// The progression of an account, all 0 if it has none yet.
[[nodiscard]] AccountProgress readProgress(rtype::database::Database &database, long long accountId);

/// Adds a game to the progression of an account, in a transaction.
/// @return The progression with it.
AccountProgress addGame(rtype::database::Database &database, long long accountId, const GameResult &game);

} // namespace rtype::server
