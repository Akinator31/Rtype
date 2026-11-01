#include "Web/AdminJson.hpp"

#include <cstdint>
#include <utility>
#include <variant>

namespace rtype::server
{

namespace
{

std::int64_t milliseconds(std::chrono::system_clock::time_point time)
{
    return static_cast<std::int64_t>(unixMilliseconds(time));
}

std::int64_t milliseconds(std::chrono::milliseconds duration)
{
    return static_cast<std::int64_t>(duration.count());
}

/// The details of an action, recorded as a JSON object: an object again, or empty if it is not one.
crow::json::wvalue details(const std::string &text)
{
    const crow::json::rvalue read = crow::json::load(text);
    if (!read || read.t() != crow::json::type::Object) {
        return crow::json::wvalue::object{};
    }
    return crow::json::wvalue(read);
}

crow::json::wvalue event(std::string_view type, std::string_view key, crow::json::wvalue value)
{
    crow::json::wvalue json;
    json["type"] = std::string(type);
    json[std::string(key)] = std::move(value);
    return json;
}

} // namespace

std::string toString(GameEnd reason)
{
    switch (reason) {
    case GameEnd::Empty:
        return "empty";
    case GameEnd::Over:
        return "over";
    case GameEnd::Crashed:
        return "crashed";
    case GameEnd::EndedByAdmin:
        return "ended_by_admin";
    case GameEnd::MasterStopping:
        return "master_stopping";
    }
    return "unknown";
}

crow::json::wvalue toJson(const AdminServer &server)
{
    crow::json::wvalue json;
    json["version"] = server.version;
    json["startedAt"] = milliseconds(server.startedAt);
    json["uptime"] = milliseconds(server.uptime);
    json["ports"]["lobby"] = server.lobbyPort;
    json["ports"]["games"]["first"] = server.firstGamePort;
    json["ports"]["games"]["last"] = server.lastGamePort;
    json["ports"]["panel"] = server.panelPort;
    json["players"] = static_cast<std::uint64_t>(server.players);
    json["games"] = static_cast<std::uint64_t>(server.games);
    json["bans"] = static_cast<std::uint64_t>(server.bans);
    return json;
}

crow::json::wvalue toJson(const AdminGame &game)
{
    crow::json::wvalue json;
    json["id"] = game.id;
    json["name"] = game.name;
    json["maxPlayers"] = game.maxPlayers;
    json["port"] = game.port;
    json["createdAt"] = milliseconds(game.createdAt);
    json["tick"] = game.status ? crow::json::wvalue(game.status->tick) : crow::json::wvalue(nullptr);
    json["wave"] = game.status ? crow::json::wvalue(game.status->wave) : crow::json::wvalue(nullptr);
    json["statusAge"] = milliseconds(game.statusAge);
    json["stale"] = game.stale;
    std::vector<crow::json::wvalue> players;
    for (const AdminGamePlayer &member : game.players) {
        crow::json::wvalue player;
        player["id"] = member.id;
        player["name"] = member.name;
        // Until its client connects, its instance does not report it
        const PlayerStatus *status = nullptr;
        if (game.status) {
            for (const PlayerStatus &reported : game.status->players) {
                status = reported.playerId == member.id ? &reported : status;
            }
        }
        player["score"] = status != nullptr ? crow::json::wvalue(status->score) : crow::json::wvalue(nullptr);
        player["alive"] = status != nullptr ? crow::json::wvalue(status->alive) : crow::json::wvalue(nullptr);
        player["latency"] = status != nullptr && status->latency ? crow::json::wvalue(*status->latency)
                                                                 : crow::json::wvalue(nullptr);
        players.push_back(std::move(player));
    }
    json["players"] = std::move(players);
    return json;
}

crow::json::wvalue toJson(const AdminPlayer &player)
{
    crow::json::wvalue json;
    json["id"] = player.id;
    json["name"] = player.name;
    json["address"] = player.address.ip();
    json["gameId"] = player.gameId ? crow::json::wvalue(*player.gameId) : crow::json::wvalue(nullptr);
    json["joinedAt"] = milliseconds(player.joinedAt);
    json["accountId"] = player.accountId ? crow::json::wvalue(static_cast<std::int64_t>(*player.accountId))
                                         : crow::json::wvalue(nullptr);
    return json;
}

crow::json::wvalue discordJson(const std::optional<DiscordProfile> &profile)
{
    if (!profile) {
        return crow::json::wvalue(nullptr);
    }
    crow::json::wvalue json;
    json["username"] = profile->username;
    json["displayName"] =
        profile->displayName.empty() ? crow::json::wvalue(nullptr) : crow::json::wvalue(profile->displayName);
    json["avatar"] =
        profile->avatarHash.empty() ? crow::json::wvalue(nullptr) : crow::json::wvalue(profile->avatarHash);
    return json;
}

crow::json::wvalue toJson(const AdminAccount &adminAccount)
{
    const Account &account = adminAccount.entry.account;
    const auto optionalTime = [](const std::optional<std::chrono::system_clock::time_point> &time) {
        return time ? crow::json::wvalue(milliseconds(*time)) : crow::json::wvalue(nullptr);
    };
    crow::json::wvalue json;
    json["id"] = static_cast<std::int64_t>(account.id);
    json["username"] = account.username;
    json["email"] = account.email ? crow::json::wvalue(*account.email) : crow::json::wvalue(nullptr);
    json["emailVerifiedAt"] = optionalTime(account.emailVerifiedAt);
    json["password"] = account.passwordHash.has_value();
    json["discordId"] = adminAccount.entry.discordId ? crow::json::wvalue(*adminAccount.entry.discordId)
                                                     : crow::json::wvalue(nullptr);
    json["discord"] = discordJson(adminAccount.entry.discord);
    json["role"] = std::string(roleName(adminAccount.locked ? AccountRole::Admin : account.role));
    json["locked"] = adminAccount.locked;
    json["bannedAt"] = optionalTime(account.bannedAt);
    json["banReason"] = account.banReason;
    json["createdAt"] = milliseconds(account.createdAt);
    json["lastSeenAt"] = milliseconds(account.lastSeenAt);
    return json;
}

crow::json::wvalue toJson(const AdminAction &action)
{
    crow::json::wvalue json;
    json["id"] = static_cast<std::int64_t>(action.id);
    json["at"] = milliseconds(action.at);
    json["actor"] = action.actor;
    json["action"] = action.action;
    json["target"] = action.target;
    json["details"] = details(action.details);
    return json;
}

crow::json::wvalue toJson(const AddressBan &ban)
{
    crow::json::wvalue json;
    json["address"] = ban.address.ip();
    json["createdAt"] = milliseconds(ban.createdAt);
    json["expiresAt"] = milliseconds(ban.expiresAt);
    json["reason"] = ban.reason;
    json["actor"] = ban.actor;
    return json;
}

crow::json::wvalue toJson(const AdminEvent &adminEvent)
{
    return std::visit(
        [](const auto &happened) -> crow::json::wvalue {
            using Event = std::decay_t<decltype(happened)>;
            if constexpr (std::is_same_v<Event, admin_event::Snapshot>) {
                crow::json::wvalue json;
                json["type"] = "snapshot";
                json["server"] = toJson(happened.server);
                json["games"] = toJsonList(happened.games);
                json["players"] = toJsonList(happened.players);
                return json;
            } else if constexpr (std::is_same_v<Event, admin_event::PlayerJoined>) {
                return event("player.joined", "player", toJson(happened.player));
            } else if constexpr (std::is_same_v<Event, admin_event::PlayerUpdated>) {
                return event("player.updated", "player", toJson(happened.player));
            } else if constexpr (std::is_same_v<Event, admin_event::PlayerLeft>) {
                return event("player.left", "playerId", crow::json::wvalue(happened.playerId));
            } else if constexpr (std::is_same_v<Event, admin_event::GameStarted>) {
                return event("game.started", "game", toJson(happened.game));
            } else if constexpr (std::is_same_v<Event, admin_event::GameUpdated>) {
                return event("game.updated", "game", toJson(happened.game));
            } else if constexpr (std::is_same_v<Event, admin_event::GameEnded>) {
                crow::json::wvalue json = event("game.ended", "gameId", crow::json::wvalue(happened.gameId));
                json["reason"] = toString(happened.reason);
                return json;
            } else {
                return event("action", "action", toJson(happened.action));
            }
        },
        adminEvent);
}

} // namespace rtype::server
