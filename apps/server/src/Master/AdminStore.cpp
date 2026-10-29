#include "Master/AdminStore.hpp"

#include <soci/soci.h>

#include <limits>

namespace rtype::server
{

long long unixMilliseconds(std::chrono::system_clock::time_point time)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

std::chrono::system_clock::time_point fromUnixMilliseconds(long long milliseconds)
{
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(milliseconds));
}

long long recordAction(rtype::database::Database &database, const AdminAction &action)
{
    const long long at = unixMilliseconds(action.at);
    long long id = 0;
    // One writer, the database thread: the next id cannot be taken meanwhile
    database.session()
        << "INSERT INTO admin_actions (id, at, actor, action, target, details) VALUES ((SELECT "
           "COALESCE(MAX(id), 0) + 1 FROM admin_actions), :at, :actor, :action, :target, "
           ":details) RETURNING id",
        soci::use(at), soci::use(action.actor), soci::use(action.action), soci::use(action.target),
        soci::use(action.details), soci::into(id);
    return id;
}

std::vector<AdminAction> readActions(rtype::database::Database &database, std::size_t limit,
                                     std::optional<long long> before)
{
    const long long last = before.value_or(std::numeric_limits<long long>::max());
    const auto count = static_cast<long long>(limit);
    const soci::rowset<soci::row> rows =
        (database.session().prepare << "SELECT id, at, actor, action, target, details FROM admin_actions "
                                       "WHERE id < :last ORDER BY id DESC LIMIT :count",
         soci::use(last), soci::use(count));
    std::vector<AdminAction> actions;
    for (const soci::row &row : rows) {
        actions.push_back({
            .id = row.get<long long>(0),
            .at = fromUnixMilliseconds(row.get<long long>(1)),
            .actor = row.get<std::string>(2),
            .action = row.get<std::string>(3),
            .target = row.get<std::string>(4),
            .details = row.get<std::string>(5),
        });
    }
    return actions;
}

void saveAddressBan(rtype::database::Database &database, const AddressBan &ban)
{
    const std::string address = banKey(ban.address).ip();
    const long long createdAt = unixMilliseconds(ban.createdAt);
    const long long expiresAt = unixMilliseconds(ban.expiresAt);
    database.session()
        << "INSERT INTO address_bans (address, created_at, expires_at, reason, actor) VALUES "
           "(:address, :created, :expires, :reason, :actor) ON CONFLICT (address) DO UPDATE SET "
           "created_at = excluded.created_at, expires_at = excluded.expires_at, "
           "reason = excluded.reason, actor = excluded.actor",
        soci::use(address), soci::use(createdAt), soci::use(expiresAt), soci::use(ban.reason),
        soci::use(ban.actor);
}

std::size_t purgeAddressBans(rtype::database::Database &database, std::chrono::system_clock::time_point now)
{
    const long long at = unixMilliseconds(now);
    soci::statement statement =
        (database.session().prepare << "DELETE FROM address_bans WHERE expires_at <= :now", soci::use(at));
    statement.execute(true);
    return static_cast<std::size_t>(statement.get_affected_rows());
}

std::vector<AddressBan> loadAddressBans(rtype::database::Database &database,
                                        std::chrono::system_clock::time_point now)
{
    purgeAddressBans(database, now);
    const soci::rowset<soci::row> rows =
        (database.session().prepare
         << "SELECT address, created_at, expires_at, reason, actor FROM address_bans ORDER BY address");
    std::vector<AddressBan> bans;
    for (const soci::row &row : rows) {
        const std::optional<rtype::network::Address> address =
            rtype::network::Address::parse(row.get<std::string>(0), 0);
        if (!address) {
            // Written by this server, so never: skipped rather than refusing to start
            continue;
        }
        bans.push_back({
            .address = *address,
            .createdAt = fromUnixMilliseconds(row.get<long long>(1)),
            .expiresAt = fromUnixMilliseconds(row.get<long long>(2)),
            .reason = row.get<std::string>(3),
            .actor = row.get<std::string>(4),
        });
    }
    return bans;
}

} // namespace rtype::server
