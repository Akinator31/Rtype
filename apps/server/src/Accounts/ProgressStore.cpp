#include "Accounts/ProgressStore.hpp"

#include <soci/soci.h>

#include <algorithm>
#include <limits>

namespace rtype::server
{

namespace
{

/// Kept as BIGINT, the integers both engines have.
long long toColumn(std::uint64_t value)
{
    return static_cast<long long>(std::min<std::uint64_t>(value, std::numeric_limits<long long>::max()));
}

template <typename Value>
Value fromColumn(long long value)
{
    if (value <= 0) {
        return 0;
    }
    return static_cast<Value>(
        std::min<std::uint64_t>(static_cast<std::uint64_t>(value), std::numeric_limits<Value>::max()));
}

template <typename Value>
Value add(Value total, std::uint64_t amount)
{
    return static_cast<Value>(std::min<std::uint64_t>(total + amount, std::numeric_limits<Value>::max()));
}

} // namespace

AccountProgress readProgress(rtype::database::Database &database, long long accountId)
{
    long long xp = 0;
    long long games = 0;
    long long kills = 0;
    long long waves = 0;
    long long streak = 0;
    database.session()
        << "SELECT xp, games_played, total_kills, best_waves, best_streak FROM account_progress "
           "WHERE account_id = :id",
        soci::use(accountId), soci::into(xp), soci::into(games), soci::into(kills), soci::into(waves),
        soci::into(streak);
    if (!database.session().got_data()) {
        return {};
    }
    return {.xp = fromColumn<std::uint64_t>(xp),
            .gamesPlayed = fromColumn<std::uint32_t>(games),
            .totalKills = fromColumn<std::uint32_t>(kills),
            .bestWaves = fromColumn<std::uint16_t>(waves),
            .bestStreak = fromColumn<std::uint16_t>(streak)};
}

AccountProgress addGame(rtype::database::Database &database, long long accountId, const GameResult &game)
{
    return database.transaction([&] {
        AccountProgress progress = readProgress(database, accountId);
        const bool known = progress.gamesPlayed > 0;
        progress.xp = std::min<std::uint64_t>(progress.xp + game.xp, std::numeric_limits<long long>::max());
        progress.gamesPlayed = add(progress.gamesPlayed, 1);
        progress.totalKills = add(progress.totalKills, game.kills);
        progress.bestWaves = std::max(progress.bestWaves, game.wavesCleared);
        progress.bestStreak = std::max(progress.bestStreak, game.bestStreak);

        long long xp = toColumn(progress.xp);
        long long games = progress.gamesPlayed;
        long long kills = progress.totalKills;
        long long waves = progress.bestWaves;
        long long streak = progress.bestStreak;
        if (known) {
            database.session() << "UPDATE account_progress SET xp = :xp, games_played = :games, "
                                  "total_kills = :kills, best_waves = :waves, best_streak = :streak "
                                  "WHERE account_id = :id",
                soci::use(xp), soci::use(games), soci::use(kills), soci::use(waves), soci::use(streak),
                soci::use(accountId);
        } else {
            database.session()
                << "INSERT INTO account_progress (account_id, xp, games_played, total_kills, "
                   "best_waves, best_streak) VALUES (:id, :xp, :games, :kills, :waves, :streak)",
                soci::use(accountId), soci::use(xp), soci::use(games), soci::use(kills), soci::use(waves),
                soci::use(streak);
        }
        return progress;
    });
}

} // namespace rtype::server
