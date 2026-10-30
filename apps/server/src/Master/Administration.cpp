#include "Master/Administration.hpp"

#include "Accounts/AccountRules.hpp"
#include "rtype/log/Logger.hpp"

#include <crow/json.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <utility>

namespace rtype::server
{

namespace
{

using Clock = std::chrono::system_clock;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("admin");
    return instance;
}

/// `, because "…"` when a reason is given, for the logs.
std::string because(const std::string &reason)
{
    return reason.empty() ? std::string{} : std::format(", because \"{}\"", reason);
}

} // namespace

Administration::Administration(rtype::network::EventLoop &loop, AdminGames &games, AdminLobby &lobby,
                               AddressBans &bans, rtype::database::DatabaseThread *database,
                               AdministrationSettings settings)
    : _games(games), _lobby(lobby), _bans(bans), _database(database), _settings(std::move(settings)),
      _purge(loop)
{
    _purge.startRepeating(PurgeInterval, [this] { purge(); });
}

Administration::~Administration()
{
    _purge.cancel();
}

// --- What it shows ---

AdminServer Administration::server() const
{
    return {
        .version = _settings.version,
        .startedAt = _startedAt,
        .uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                        _started),
        .lobbyPort = _settings.lobbyPort,
        .firstGamePort = _settings.firstGamePort,
        .lastGamePort = _settings.lastGamePort,
        .panelPort = _settings.panelPort,
        .players = _lobby.adminPlayers().size(),
        .games = _games.adminGames().size(),
        .bans = _bans.list().size(),
    };
}

std::optional<AdminGame> Administration::game(std::uint32_t gameId) const
{
    for (AdminGame &game : _games.adminGames()) {
        if (game.id == gameId) {
            return std::move(game);
        }
    }
    return std::nullopt;
}

std::optional<AdminPlayer> Administration::findPlayer(std::uint32_t playerId) const
{
    for (AdminPlayer &player : _lobby.adminPlayers()) {
        if (player.id == playerId) {
            return std::move(player);
        }
    }
    return std::nullopt;
}

void Administration::actions(std::size_t limit, std::optional<long long> before, ActionsCallback done)
{
    if (_database == nullptr) {
        done(std::vector<AdminAction>{});
        return;
    }
    auto shared = std::make_shared<ActionsCallback>(std::move(done));
    const bool queued = _database->run(
        "read the actions of the administrators",
        [limit, before](rtype::database::Database &database) { return readActions(database, limit, before); },
        [shared, alive = std::weak_ptr<bool>(_alive)](std::optional<std::vector<AdminAction>> actions) {
            if (!alive.expired()) {
                (*shared)(std::move(actions));
            }
        });
    if (!queued) {
        (*shared)(std::nullopt);
    }
}

// --- What it does ---

void Administration::record(std::string actor, std::string action, std::string target, std::string details)
{
    AdminAction recorded{
        .id = 0,
        // To the millisecond, as the database keeps it: the event and the record say the same
        .at = std::chrono::floor<std::chrono::milliseconds>(Clock::now()),
        .actor = std::move(actor),
        .action = std::move(action),
        .target = std::move(target),
        .details = std::move(details),
    };
    if (_database == nullptr) {
        publish(admin_event::ActionRecorded{std::move(recorded)});
        return;
    }
    const bool queued = _database->run(
        "record an action of an administrator",
        [recorded](rtype::database::Database &database) { return recordAction(database, recorded); },
        [this, recorded, alive = std::weak_ptr<bool>(_alive)](std::optional<long long> id) mutable {
            if (alive.expired()) {
                return;
            }
            if (!id) {
                logger().error("The {} on {} could not be recorded", recorded.action, recorded.target);
                return;
            }
            recorded.id = *id;
            publish(admin_event::ActionRecorded{std::move(recorded)});
        });
    if (!queued) {
        logger().error("The {} on {} could not be recorded: the database is too busy", recorded.action,
                       recorded.target);
    }
}

bool Administration::kick(std::uint32_t playerId, const std::string &reason, const std::string &actor)
{
    const std::optional<AdminPlayer> player = findPlayer(playerId);
    if (!player) {
        return false;
    }
    logger().info("{} ({}) removed by {}{}", player->name, playerId, actor, because(reason));
    // From its game first, with the reason KICKED for its instance; then from the lobby
    _games.kick(playerId);
    _lobby.disconnect(playerId, rtype::lobby::ErrorCode::Kicked);

    crow::json::wvalue details;
    details["name"] = player->name;
    details["address"] = player->address.ip();
    if (player->accountId) {
        // Its name and address are taken out if the account is deleted
        details["account"] = static_cast<std::int64_t>(*player->accountId);
    }
    if (player->gameId) {
        details["gameId"] = *player->gameId;
    }
    details["reason"] = reason;
    record(actor, "kick", std::format("player:{}", playerId), details.dump());
    return true;
}

bool Administration::endGame(std::uint32_t gameId, const std::string &reason, const std::string &actor)
{
    const std::optional<AdminGame> ended = game(gameId);
    if (!ended || !_games.end(gameId)) {
        return false;
    }
    logger().info("Game {} \"{}\" ended by {}{}", gameId, ended->name, actor, because(reason));

    crow::json::wvalue details;
    details["name"] = ended->name;
    std::vector<crow::json::wvalue> players;
    std::vector<crow::json::wvalue> accounts;
    for (const AdminGamePlayer &player : ended->players) {
        players.emplace_back(player.name);
        if (const std::optional<AdminPlayer> signedIn = findPlayer(player.id);
            signedIn && signedIn->accountId) {
            accounts.emplace_back(static_cast<std::int64_t>(*signedIn->accountId));
        }
    }
    details["players"] = std::move(players);
    // The players signed in: their names are taken out if their account is deleted
    details["accounts"] = std::move(accounts);
    details["reason"] = reason;
    record(actor, "end_game", std::format("game:{}", gameId), details.dump());
    return true;
}

std::optional<std::size_t> Administration::sendMessage(std::optional<std::uint32_t> gameId,
                                                       const std::string &text, const std::string &actor)
{
    if (gameId && !game(*gameId)) {
        return std::nullopt;
    }
    std::size_t recipients = 0;
    for (const AdminPlayer &player : _lobby.adminPlayers()) {
        if (!gameId || player.gameId == gameId) {
            recipients += _lobby.sendMessage(player.id, text) ? 1 : 0;
        }
    }
    const std::string target = gameId ? std::format("game:{}", *gameId) : "everyone";
    logger().info("Message from {} to {} ({} players): {}", actor, target, recipients, text);

    crow::json::wvalue details;
    details["text"] = text;
    details["recipients"] = static_cast<std::uint64_t>(recipients);
    record(actor, "message", target, details.dump());
    return recipients;
}

std::size_t Administration::ban(const rtype::network::Address &address, std::chrono::seconds duration,
                                const std::string &reason, const std::string &actor)
{
    const Clock::time_point now = Clock::now();
    const AddressBan ban{
        .address = banKey(address),
        .createdAt = now,
        .expiresAt = now + std::min<std::chrono::seconds>(duration, MaxBanDuration),
        .reason = reason,
        .actor = actor,
    };
    _bans.add(ban);
    if (_database != nullptr &&
        !_database->run("keep the ban of an address",
                        [ban](rtype::database::Database &database) { saveAddressBan(database, ban); })) {
        logger().error("The ban of {} is not kept across restarts: the database is too busy",
                       ban.address.ip());
    }

    std::size_t removed = 0;
    for (const AdminPlayer &player : _lobby.adminPlayers()) {
        if (player.address == ban.address) {
            _games.kick(player.id);
            removed += _lobby.disconnect(player.id, rtype::lobby::ErrorCode::Banned) ? 1 : 0;
        }
    }
    logger().info("{} banned by {} for {} s{}: {} players removed", ban.address.ip(), actor,
                  std::chrono::duration_cast<std::chrono::seconds>(ban.expiresAt - now).count(),
                  because(reason), removed);

    crow::json::wvalue details;
    details["duration"] = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(ban.expiresAt - now).count());
    details["expiresAt"] = static_cast<std::int64_t>(unixMilliseconds(ban.expiresAt));
    details["reason"] = reason;
    details["removed"] = static_cast<std::uint64_t>(removed);
    record(actor, "ban", std::format("address:{}", ban.address.ip()), details.dump());
    return removed;
}

bool Administration::unban(const rtype::network::Address &address, const std::string &reason,
                           const std::string &actor)
{
    const std::optional<AddressBan> lifted = _bans.remove(address);
    if (!lifted) {
        return false;
    }
    if (_database != nullptr && !_database->run("lift the ban of an address",
                                                [key = lifted->address](rtype::database::Database &database) {
                                                    deleteAddressBan(database, key);
                                                })) {
        logger().error("The ban of {} is lifted until the next start only: the database is too busy",
                       lifted->address.ip());
    }
    logger().info("The ban of {} lifted by {}{}", lifted->address.ip(), actor, because(reason));

    crow::json::wvalue details;
    details["expiresAt"] = static_cast<std::int64_t>(unixMilliseconds(lifted->expiresAt));
    details["bannedBy"] = lifted->actor;
    details["reason"] = reason;
    record(actor, "unban", std::format("address:{}", lifted->address.ip()), details.dump());
    return true;
}

// --- The accounts ---

AdminAccount Administration::adminAccount(AccountEntry entry) const
{
    const bool locked = isListedAdmin(
        _settings.admins, entry.account.verified() ? entry.account.email : std::nullopt, entry.discordId);
    return {.entry = std::move(entry), .locked = locked};
}

void Administration::accounts(std::string search, std::size_t limit, std::optional<long long> before,
                              AccountsCallback done)
{
    if (_database == nullptr) {
        done(std::vector<AdminAccount>{});
        return;
    }
    auto shared = std::make_shared<AccountsCallback>(std::move(done));
    const bool queued = _database->run(
        "read the accounts",
        [search = std::move(search), limit, before](rtype::database::Database &database) {
            return listAccounts(database, search, limit, before);
        },
        [this, shared,
         alive = std::weak_ptr<bool>(_alive)](std::optional<std::vector<AccountEntry>> entries) {
            if (alive.expired()) {
                return;
            }
            if (!entries) {
                (*shared)(std::nullopt);
                return;
            }
            std::vector<AdminAccount> accounts;
            accounts.reserve(entries->size());
            for (AccountEntry &entry : *entries) {
                accounts.push_back(adminAccount(std::move(entry)));
            }
            (*shared)(std::move(accounts));
        });
    if (!queued) {
        (*shared)(std::nullopt);
    }
}

void Administration::account(long long accountId, AccountCallback done)
{
    if (_database == nullptr) {
        done(std::optional<AdminAccount>{});
        return;
    }
    auto shared = std::make_shared<AccountCallback>(std::move(done));
    const bool queued = _database->run(
        "read an account",
        [accountId](rtype::database::Database &database) { return findAccountEntry(database, accountId); },
        [this, shared,
         alive = std::weak_ptr<bool>(_alive)](std::optional<std::optional<AccountEntry>> entry) {
            if (alive.expired()) {
                return;
            }
            if (!entry) {
                (*shared)(std::nullopt);
                return;
            }
            (*shared)(*entry ? std::optional(adminAccount(std::move(**entry))) : std::nullopt);
        });
    if (!queued) {
        (*shared)(std::nullopt);
    }
}

void Administration::changeAccount(
    std::string description, long long accountId, bool refuseLocked,
    std::function<void(rtype::database::Database &, const AccountEntry &)> change, ChangeCallback done)
{
    using Outcome = AccountChanged::Outcome;
    if (_database == nullptr) {
        done({.outcome = Outcome::NoAccount, .account = std::nullopt, .removed = 0});
        return;
    }
    struct Result {
        Outcome outcome = Outcome::Done;
        std::optional<AccountEntry> before;
        std::optional<AccountEntry> after;
    };
    auto shared = std::make_shared<ChangeCallback>(std::move(done));
    const bool queued = _database->run(
        std::move(description),
        [accountId, refuseLocked, admins = _settings.admins,
         change = std::move(change)](rtype::database::Database &database) {
            // The database thread is its only writer: the account cannot change between the reads and the
            // change, whose own functions take their transactions
            Result result;
            result.before = findAccountEntry(database, accountId);
            if (!result.before) {
                result.outcome = Outcome::NoAccount;
                return result;
            }
            const Account &account = result.before->account;
            if (refuseLocked && isListedAdmin(admins, account.verified() ? account.email : std::nullopt,
                                              result.before->discordId)) {
                result.outcome = Outcome::Locked;
                return result;
            }
            change(database, *result.before);
            result.after = findAccountEntry(database, accountId);
            return result;
        },
        [this, shared, alive = std::weak_ptr<bool>(_alive)](std::optional<Result> result) {
            if (alive.expired()) {
                return;
            }
            if (!result) {
                (*shared)({.outcome = Outcome::Failed, .account = std::nullopt, .removed = 0});
                return;
            }
            std::optional<AdminAccount> account;
            if (result->after) {
                account = adminAccount(std::move(*result->after));
            } else if (result->outcome != Outcome::Done && result->before) {
                account = adminAccount(std::move(*result->before));
            }
            (*shared)({.outcome = result->outcome, .account = std::move(account), .removed = 0});
        });
    if (!queued) {
        (*shared)({.outcome = Outcome::Failed, .account = std::nullopt, .removed = 0});
    }
}

std::size_t Administration::removeAccountPlayers(long long accountId, rtype::lobby::ErrorCode code)
{
    std::size_t removed = 0;
    for (const AdminPlayer &player : _lobby.adminPlayers()) {
        if (player.accountId == accountId) {
            _games.kick(player.id);
            removed += _lobby.disconnect(player.id, code) ? 1 : 0;
        }
    }
    return removed;
}

void Administration::banAccount(long long accountId, const std::string &reason, const std::string &actor,
                                ChangeCallback done)
{
    changeAccount(
        "ban an account", accountId, true,
        [reason](rtype::database::Database &database, const AccountEntry &entry) {
            rtype::server::banAccount(database, entry.account.id, reason, std::chrono::system_clock::now());
        },
        [this, accountId, reason, actor, done = std::move(done)](AccountChanged changed) {
            if (changed.outcome == AccountChanged::Outcome::Done && changed.account) {
                // Its sessions are ended: its connections go too
                changed.removed = removeAccountPlayers(accountId, rtype::lobby::ErrorCode::Banned);
                const std::string &name = changed.account->entry.account.username;
                logger().info("Account {} ({}) banned by {}{}: {} players removed", name, accountId, actor,
                              because(reason), changed.removed);
                crow::json::wvalue details;
                details["account"] = static_cast<std::int64_t>(accountId);
                details["name"] = name;
                details["reason"] = reason;
                details["removed"] = static_cast<std::uint64_t>(changed.removed);
                record(actor, "ban_account", std::format("account:{}", accountId), details.dump());
            }
            done(std::move(changed));
        });
}

void Administration::unbanAccount(long long accountId, const std::string &reason, const std::string &actor,
                                  ChangeCallback done)
{
    changeAccount(
        "lift the ban of an account", accountId, false,
        [](rtype::database::Database &database, const AccountEntry &entry) {
            rtype::server::unbanAccount(database, entry.account.id);
        },
        [this, accountId, reason, actor, done = std::move(done)](AccountChanged changed) {
            if (changed.outcome == AccountChanged::Outcome::Done && changed.account) {
                const std::string &name = changed.account->entry.account.username;
                logger().info("The ban of account {} ({}) lifted by {}{}", name, accountId, actor,
                              because(reason));
                crow::json::wvalue details;
                details["account"] = static_cast<std::int64_t>(accountId);
                details["name"] = name;
                details["reason"] = reason;
                record(actor, "unban_account", std::format("account:{}", accountId), details.dump());
            }
            done(std::move(changed));
        });
}

void Administration::setRole(long long accountId, AccountRole role, const std::string &actor,
                             ChangeCallback done)
{
    // Making a locked account an administrator changes nothing; demoting it is refused
    changeAccount(
        "change the role of an account", accountId, role != AccountRole::Admin,
        [role](rtype::database::Database &database, const AccountEntry &entry) {
            rtype::server::setRole(database, entry.account.id, role);
        },
        [this, accountId, role, actor, done = std::move(done)](AccountChanged changed) {
            if (changed.outcome == AccountChanged::Outcome::Done && changed.account) {
                const std::string &name = changed.account->entry.account.username;
                logger().info("Account {} ({}) made {} by {}", name, accountId, roleName(role), actor);
                crow::json::wvalue details;
                details["account"] = static_cast<std::int64_t>(accountId);
                details["name"] = name;
                details["role"] = std::string(roleName(role));
                record(actor, "role", std::format("account:{}", accountId), details.dump());
            }
            done(std::move(changed));
        });
}

void Administration::deleteAccount(long long accountId, const std::string &reason, const std::string &actor,
                                   ChangeCallback done)
{
    changeAccount(
        "delete an account", accountId, true,
        [](rtype::database::Database &database, const AccountEntry &entry) {
            rtype::server::deleteAccount(database, entry.account.id);
        },
        [this, accountId, reason, actor, done = std::move(done)](AccountChanged changed) {
            if (changed.outcome == AccountChanged::Outcome::Done) {
                changed.removed = removeAccountPlayers(accountId, rtype::lobby::ErrorCode::Kicked);
                logger().info("Account {} deleted by {}{}: {} players removed", accountId, actor,
                              because(reason), changed.removed);
                // Neither its name nor its address: the account is gone
                crow::json::wvalue details;
                details["reason"] = reason;
                details["removed"] = static_cast<std::uint64_t>(changed.removed);
                record(actor, "delete_account", std::format("account:{}", accountId), details.dump());
            }
            done(std::move(changed));
        });
}

void Administration::purge()
{
    const Clock::time_point now = Clock::now();
    if (_bans.purge(now) == 0 || _database == nullptr) {
        return;
    }
    (void)_database->run("purge the expired bans",
                         [now](rtype::database::Database &database) { purgeAddressBans(database, now); });
}

// --- The events ---

std::uint64_t Administration::subscribe(Subscriber subscriber)
{
    const std::uint64_t id = ++_lastSubscriber;
    subscriber(admin_event::Snapshot{server(), _games.adminGames(), _lobby.adminPlayers()});
    _subscribers.emplace(id, std::move(subscriber));
    return id;
}

void Administration::unsubscribe(std::uint64_t id)
{
    _subscribers.erase(id);
}

void Administration::publish(const AdminEvent &event)
{
    // A subscriber may unsubscribe, itself or another, while it is told
    std::vector<std::uint64_t> ids;
    ids.reserve(_subscribers.size());
    for (const auto &[id, subscriber] : _subscribers) {
        ids.push_back(id);
    }
    for (const std::uint64_t id : ids) {
        const auto found = _subscribers.find(id);
        if (found != _subscribers.end()) {
            const Subscriber subscriber = found->second;
            subscriber(event);
        }
    }
}

void Administration::playerJoined(std::uint32_t playerId)
{
    if (std::optional<AdminPlayer> player = findPlayer(playerId)) {
        publish(admin_event::PlayerJoined{std::move(*player)});
    }
}

void Administration::playerChanged(std::uint32_t playerId)
{
    if (std::optional<AdminPlayer> player = findPlayer(playerId)) {
        publish(admin_event::PlayerUpdated{std::move(*player)});
    }
}

void Administration::playerLeft(std::uint32_t playerId)
{
    publish(admin_event::PlayerLeft{playerId});
}

void Administration::gameStarted(std::uint32_t gameId)
{
    if (std::optional<AdminGame> started = game(gameId)) {
        publish(admin_event::GameStarted{std::move(*started)});
    }
}

void Administration::gameChanged(std::uint32_t gameId)
{
    if (std::optional<AdminGame> changed = game(gameId)) {
        publish(admin_event::GameUpdated{std::move(*changed)});
    }
}

void Administration::gameEnded(std::uint32_t gameId, GameEnd reason)
{
    publish(admin_event::GameEnded{gameId, reason});
}

} // namespace rtype::server
