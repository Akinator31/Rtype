#include "Master/AdminStore.hpp"

#include <crow/json.h>
#include <soci/soci.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

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

namespace
{

/// The details of an action done to the account, without what tells who it was; nothing if the action was
/// not done to it.
std::optional<std::string> withoutAccount(const std::string &action, const std::string &text,
                                          long long accountId, std::string_view username)
{
    const crow::json::rvalue details = crow::json::load(text);
    if (!details || details.t() != crow::json::type::Object) {
        return std::nullopt;
    }
    const auto isAccount = [&](const crow::json::rvalue &value) {
        return value.t() == crow::json::type::Number && value.i() == accountId;
    };
    crow::json::wvalue result;
    // A player removed, an account banned, its ban lifted, its role changed: the account in `account`
    if (action != "end_game" && details.has("account") && isAccount(details["account"])) {
        for (const std::string &key : details.keys()) {
            if (key != "account" && key != "name" && key != "address") {
                result[key] = crow::json::wvalue(details[key]);
            }
        }
        result["name"] = std::string(DeletedActor);
        return result.dump();
    }
    if (action == "end_game" && details.has("accounts") &&
        details["accounts"].t() == crow::json::type::List) {
        std::vector<crow::json::wvalue> accounts;
        bool found = false;
        for (const crow::json::rvalue &account : details["accounts"].lo()) {
            if (isAccount(account)) {
                found = true;
            } else {
                accounts.emplace_back(account);
            }
        }
        if (!found) {
            return std::nullopt;
        }
        for (const std::string &key : details.keys()) {
            if (key != "accounts" && key != "players") {
                result[key] = crow::json::wvalue(details[key]);
            }
        }
        std::vector<crow::json::wvalue> players;
        bool replaced = false;
        if (details.has("players") && details["players"].t() == crow::json::type::List) {
            for (const crow::json::rvalue &player : details["players"].lo()) {
                if (!replaced && player.t() == crow::json::type::String && std::string(player) == username) {
                    replaced = true;
                    players.emplace_back(std::string(DeletedActor));
                } else {
                    players.emplace_back(player);
                }
            }
        }
        result["players"] = std::move(players);
        result["accounts"] = std::move(accounts);
        return result.dump();
    }
    return std::nullopt;
}

} // namespace

std::size_t anonymizeActions(rtype::database::Database &database, long long accountId,
                             std::string_view username)
{
    const std::string actor = std::to_string(accountId);
    const std::string deleted(DeletedActor);
    soci::statement byActor =
        (database.session().prepare << "UPDATE admin_actions SET actor = :deleted WHERE actor = :actor",
         soci::use(deleted), soci::use(actor));
    byActor.execute(true);
    auto changed = static_cast<std::size_t>(byActor.get_affected_rows());

    // Few actions name an account: those are read, and written again without it
    std::vector<std::pair<long long, std::string>> rewritten;
    const soci::rowset<soci::row> rows =
        (database.session().prepare << "SELECT id, action, details FROM admin_actions WHERE details LIKE "
                                       "'%\"account%'");
    for (const soci::row &row : rows) {
        if (std::optional<std::string> details =
                withoutAccount(row.get<std::string>(1), row.get<std::string>(2), accountId, username)) {
            rewritten.emplace_back(row.get<long long>(0), std::move(*details));
        }
    }
    for (const auto &[id, details] : rewritten) {
        database.session() << "UPDATE admin_actions SET details = :details WHERE id = :id",
            soci::use(details), soci::use(id);
    }
    return changed + rewritten.size();
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

void deleteAddressBan(rtype::database::Database &database, const rtype::network::Address &address)
{
    const std::string key = banKey(address).ip();
    database.session() << "DELETE FROM address_bans WHERE address = :address", soci::use(key);
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

void saveVoiceMute(rtype::database::Database &database, const VoiceMute &mute)
{
    const long long createdAt = unixMilliseconds(mute.createdAt);
    const long long expiresAt = unixMilliseconds(mute.expiresAt);
    database.session()
        << "INSERT INTO voice_mutes (account_id, created_at, expires_at, reason, actor) VALUES "
           "(:account, :created, :expires, :reason, :actor) ON CONFLICT (account_id) DO UPDATE SET "
           "created_at = excluded.created_at, expires_at = excluded.expires_at, "
           "reason = excluded.reason, actor = excluded.actor",
        soci::use(mute.accountId), soci::use(createdAt), soci::use(expiresAt), soci::use(mute.reason),
        soci::use(mute.actor);
}

void deleteVoiceMute(rtype::database::Database &database, long long accountId)
{
    database.session() << "DELETE FROM voice_mutes WHERE account_id = :account", soci::use(accountId);
}

std::size_t purgeVoiceMutes(rtype::database::Database &database, std::chrono::system_clock::time_point now)
{
    const long long at = unixMilliseconds(now);
    soci::statement statement =
        (database.session().prepare << "DELETE FROM voice_mutes WHERE expires_at <= :now", soci::use(at));
    statement.execute(true);
    return static_cast<std::size_t>(statement.get_affected_rows());
}

std::vector<VoiceMute> loadVoiceMutes(rtype::database::Database &database,
                                      std::chrono::system_clock::time_point now)
{
    purgeVoiceMutes(database, now);
    const soci::rowset<soci::row> rows =
        (database.session().prepare
         << "SELECT account_id, created_at, expires_at, reason, actor FROM voice_mutes ORDER BY account_id");
    std::vector<VoiceMute> mutes;
    for (const soci::row &row : rows) {
        mutes.push_back({
            .accountId = row.get<long long>(0),
            .createdAt = fromUnixMilliseconds(row.get<long long>(1)),
            .expiresAt = fromUnixMilliseconds(row.get<long long>(2)),
            .reason = row.get<std::string>(3),
            .actor = row.get<std::string>(4),
        });
    }
    return mutes;
}

} // namespace rtype::server
