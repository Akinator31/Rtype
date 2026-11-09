#pragma once

#include "Accounts/AccountStore.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminModel.hpp"
#include "Master/AdminStore.hpp"
#include "Master/VoiceMutes.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rtype::server
{

/// What the administration says of the server itself.
struct AdministrationSettings {
    std::string version;
    std::uint16_t lobbyPort = 0;
    /// The UDP port of every game, the one of the relay.
    std::uint16_t gamePort = 0;
    std::uint32_t maxGames = 0;
    std::uint16_t panelPort = 0;
    /// `[admin] admins`: their accounts are locked, neither demoted, banned nor deleted.
    std::vector<std::string> admins;
    /// `[voice] enabled`: the games carry the voice.
    bool voice = true;
};

/// The server, as GET /api/server shows it.
struct AdminServer {
    std::string version;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::milliseconds uptime{0};
    std::uint16_t lobbyPort = 0;
    /// The UDP port of every game, the one of the relay.
    std::uint16_t gamePort = 0;
    std::uint32_t maxGames = 0;
    std::uint16_t panelPort = 0;
    std::size_t players = 0;
    std::size_t games = 0;
    std::size_t bans = 0;
    /// The games carry the voice.
    bool voice = true;
    /// The accounts whose voice is muted now.
    std::size_t voiceMutes = 0;
};

/// An account, as the panel shows it.
struct AdminAccount {
    AccountEntry entry;
    /// One of `[admin] admins`: neither demoted, banned nor deleted from the panel.
    bool locked = false;
    /// Its voice muted in every game until a date; nothing if not.
    std::optional<VoiceMute> voiceMute;

    friend bool operator==(const AdminAccount &, const AdminAccount &) = default;
};

/// What an action on an account gives.
struct AccountChanged {
    enum class Outcome {
        Done,
        NoAccount,
        /// One of `[admin] admins`.
        Locked,
        /// The database failed, or its queue is full.
        Failed,
    };

    Outcome outcome = Outcome::Done;
    /// The account after the change; nothing once deleted.
    std::optional<AdminAccount> account;
    /// The players of the account removed from the lobby; for a mute of its voice, its players in a game it
    /// reached.
    std::size_t removed = 0;
};

/// The longest reason an administrator gives, in bytes.
inline constexpr std::size_t MaxAdminReasonLength = 200;

/// The live events of the administration (docs/protocol/admin-api.md#events). A subscriber gets a snapshot
/// first, then the events, in the order they happen.
namespace admin_event
{
struct Snapshot {
    AdminServer server;
    std::vector<AdminGame> games;
    std::vector<AdminPlayer> players;
};
struct PlayerJoined {
    AdminPlayer player;
};
/// The player entered or left a game.
struct PlayerUpdated {
    AdminPlayer player;
};
struct PlayerLeft {
    std::uint32_t playerId = 0;
};
struct GameStarted {
    AdminGame game;
};
/// A player came or went, or the instance sent its status.
struct GameUpdated {
    AdminGame game;
};
struct GameEnded {
    std::uint32_t gameId = 0;
    GameEnd reason = GameEnd::Empty;
};
/// An action was recorded.
struct ActionRecorded {
    AdminAction action;
};
} // namespace admin_event

using AdminEvent = std::variant<admin_event::Snapshot, admin_event::PlayerJoined, admin_event::PlayerUpdated,
                                admin_event::PlayerLeft, admin_event::GameStarted, admin_event::GameUpdated,
                                admin_event::GameEnded, admin_event::ActionRecorded>;

/// Everything an administrator sees and does (ADR 0025, docs/architecture/administration.md), on the thread
/// of the loop: what it shows is read from the games and the lobby, what it does goes through them, and every
/// action is recorded in `admin_actions`.
///
/// @code
/// rtype::server::Administration administration(loop, games, lobby, bans, &databaseThread, settings);
/// games.setObserver(&administration);
/// lobby.setObserver(&administration);
/// administration.kick(playerId, "cheating", rtype::server::LocalActor);
/// @endcode
///
/// - **Actions**: remove a player (from its game, then from the lobby with ERROR (KICKED)), end a game, send
/// a
///   message to a game or to everyone, ban an address for a time (the players connected from it are removed
///   with ERROR (BANNED)). Each one is recorded with its actor; the record comes back as an event once the
///   database has it.
/// - **Bans** are kept in AddressBans for the lobby, and in the database across restarts; the expired ones
/// are
///   purged every minute.
/// - **The voice**: mute a player for the rest of its game, or an account for a time in every game
///   (VoiceMutes, kept in `voice_mutes`, purged every minute too).
/// - **Accounts**, with a database: list and search them, ban and unban, change their role, delete them. The
///   accounts of `[admin] admins` are locked.
/// - **Subscribers** get a snapshot at once, then the events.
class Administration final : public AdminObserver
{
public:
    using Subscriber = std::function<void(const AdminEvent &event)>;
    using ActionsCallback = std::function<void(std::optional<std::vector<AdminAction>> actions)>;
    using AccountsCallback = std::function<void(std::optional<std::vector<AdminAccount>> accounts)>;
    /// Nothing if the database failed; then nothing inside if there is no such account.
    using AccountCallback = std::function<void(std::optional<std::optional<AdminAccount>> account)>;
    using ChangeCallback = std::function<void(AccountChanged changed)>;
    /// Nothing if the database failed; then nothing inside if the account has no Discord profile.
    using ProfileCallback = std::function<void(std::optional<std::optional<DiscordProfile>> profile)>;

    /// How often the expired bans are forgotten.
    static constexpr std::chrono::minutes PurgeInterval{1};

private:
    AdminGames &_games;
    AdminLobby &_lobby;
    AddressBans &_bans;
    rtype::database::DatabaseThread *_database;
    VoiceMutes *_voiceMutes;
    AdministrationSettings _settings;
    std::chrono::system_clock::time_point _startedAt = std::chrono::system_clock::now();
    std::chrono::steady_clock::time_point _started = std::chrono::steady_clock::now();
    rtype::network::Timer _purge;
    std::map<std::uint64_t, Subscriber> _subscribers;
    std::uint64_t _lastSubscriber = 0;
    /// Dies with the administration: the results of the database check it.
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    void publish(const AdminEvent &event);
    [[nodiscard]] std::optional<AdminPlayer> findPlayer(std::uint32_t playerId) const;
    /// Records an action, then publishes it once recorded.
    void record(std::string actor, std::string action, std::string target, std::string details);
    void purge();
    [[nodiscard]] AdminAccount adminAccount(AccountEntry entry) const;
    /// Removes the players signed in to an account from their game and the lobby, with ERROR (`code`).
    std::size_t removeAccountPlayers(long long accountId, rtype::lobby::ErrorCode code);
    /// Changes an account on the database thread: `change` gets it, then `done` comes on the loop with what
    /// the account is after it. A locked account is refused before `change` when `refuseLocked`.
    void changeAccount(std::string description, long long accountId, bool refuseLocked,
                       std::function<void(rtype::database::Database &, const AccountEntry &)> change,
                       ChangeCallback done);

public:
    /// @param database Where the actions are recorded and the bans kept; nullptr keeps nothing.
    /// @param voiceMutes The mutes of the voice of the accounts, which the games read; nullptr: no account
    ///   is muted, only players in their game.
    Administration(rtype::network::EventLoop &loop, AdminGames &games, AdminLobby &lobby, AddressBans &bans,
                   rtype::database::DatabaseThread *database, AdministrationSettings settings,
                   VoiceMutes *voiceMutes = nullptr);
    ~Administration() override;

    Administration(const Administration &) = delete;
    Administration &operator=(const Administration &) = delete;
    Administration(Administration &&) = delete;
    Administration &operator=(Administration &&) = delete;

    [[nodiscard]] AdminServer server() const;
    [[nodiscard]] std::vector<AdminGame> games() const { return _games.adminGames(); }
    [[nodiscard]] std::optional<AdminGame> game(std::uint32_t gameId) const;
    [[nodiscard]] std::vector<AdminPlayer> players() const { return _lobby.adminPlayers(); }
    [[nodiscard]] std::vector<AddressBan> bans() const { return _bans.list(); }
    /// Reads the record of the actions, newest first, on the database thread; `done` comes on the loop, empty
    /// if the database failed or its queue is full.
    void actions(std::size_t limit, std::optional<long long> before, ActionsCallback done);

    /// Reads the accounts, newest first, on the database thread (Accounts/AccountStore.hpp, listAccounts()).
    void accounts(std::string search, std::size_t limit, std::optional<long long> before,
                  AccountsCallback done);
    void account(long long accountId, AccountCallback done);
    /// The Discord profile of an account, with its picture.
    void profile(long long accountId, ProfileCallback done);
    /// Bans an account: its sessions end, its players are removed with ERROR (BANNED).
    void banAccount(long long accountId, const std::string &reason, const std::string &actor,
                    ChangeCallback done);
    void unbanAccount(long long accountId, const std::string &reason, const std::string &actor,
                      ChangeCallback done);
    /// Gives an account a role; demoting a locked one is refused.
    void setRole(long long accountId, AccountRole role, const std::string &actor, ChangeCallback done);
    /// Deletes an account, as its owner can: its players are removed with ERROR (KICKED), its actions
    /// anonymized.
    void deleteAccount(long long accountId, const std::string &reason, const std::string &actor,
                       ChangeCallback done);

    /// Removes a player from its game and from the lobby.
    /// @return False if no player has this id.
    bool kick(std::uint32_t playerId, const std::string &reason, const std::string &actor);
    /// Ends a running game.
    /// @return False if no running game has this id.
    bool endGame(std::uint32_t gameId, const std::string &reason, const std::string &actor);
    /// Sends SERVER_MESSAGE to the players of a game, or to every player of the lobby. The text must be valid
    /// (rtype::lobby::isValidServerMessage()).
    /// @return The number of players who got it; nothing if no running game has this id.
    std::optional<std::size_t> sendMessage(std::optional<std::uint32_t> gameId, const std::string &text,
                                           const std::string &actor);
    /// Bans an address for a time, at most MaxBanDuration, and removes the players connected from it.
    /// @return The number of players removed.
    std::size_t ban(const rtype::network::Address &address, std::chrono::seconds duration,
                    const std::string &reason, const std::string &actor);
    /// Lifts the ban of an address before it expires: the lobby welcomes it again.
    /// @return False if no ban of this address runs.
    bool unban(const rtype::network::Address &address, const std::string &reason, const std::string &actor);

    /// Mutes the voice of a player for the rest of its game, or lets it talk again: the game drops its voice,
    /// the others see it muted. A player whose account is muted stays muted until that ends.
    /// @return False if the player is in no running game.
    bool muteVoice(std::uint32_t playerId, bool muted, const std::string &reason, const std::string &actor);
    /// Mutes the voice of an account for a time, at most MaxVoiceMuteDuration, in every game it plays, at
    /// once and from its next game: kept in `voice_mutes`. A locked account is refused.
    void muteAccountVoice(long long accountId, std::chrono::seconds duration, const std::string &reason,
                          const std::string &actor, ChangeCallback done);
    /// Lifts the mute of the voice of an account before it ends.
    void unmuteAccountVoice(long long accountId, const std::string &reason, const std::string &actor,
                            ChangeCallback done);
    /// The accounts whose voice is muted now, the first to end first.
    [[nodiscard]] std::vector<VoiceMute> voiceMutes() const;
    /// Calls `subscriber` at once with a snapshot, then with each event.
    /// @return What unsubscribe() takes.
    std::uint64_t subscribe(Subscriber subscriber);
    void unsubscribe(std::uint64_t id);
    [[nodiscard]] std::size_t subscriberCount() const { return _subscribers.size(); }

    void playerJoined(std::uint32_t playerId) override;
    void playerChanged(std::uint32_t playerId) override;
    void playerLeft(std::uint32_t playerId) override;
    void gameStarted(std::uint32_t gameId) override;
    void gameChanged(std::uint32_t gameId) override;
    void gameEnded(std::uint32_t gameId, GameEnd reason) override;
};

} // namespace rtype::server
