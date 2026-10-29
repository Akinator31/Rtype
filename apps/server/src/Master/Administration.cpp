#include "Master/Administration.hpp"

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
    for (const AdminGamePlayer &player : ended->players) {
        players.emplace_back(player.name);
    }
    details["players"] = std::move(players);
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
