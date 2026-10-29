#pragma once

#include "Common/InstanceMessages.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/Address.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// What the administration sees of the master, and the small interfaces it acts through
/// (docs/architecture/administration.md): Games and Lobby implement them, the tests give fakes of them.
namespace rtype::server
{

/// A player of the lobby, as the administration sees it.
struct AdminPlayer {
    std::uint32_t id = 0;
    std::string name;
    /// The IP address of its connection, its port 0.
    rtype::network::Address address;
    /// The game it is in, or creating.
    std::optional<std::uint32_t> gameId;
    std::chrono::system_clock::time_point joinedAt;

    friend bool operator==(const AdminPlayer &, const AdminPlayer &) = default;
};

/// A player of a game.
struct AdminGamePlayer {
    std::uint32_t id = 0;
    std::string name;

    friend bool operator==(const AdminGamePlayer &, const AdminGamePlayer &) = default;
};

/// A running game, as the administration sees it: its instance said READY, and it is not ending.
struct AdminGame {
    std::uint32_t id = 0;
    std::string name;
    std::uint8_t maxPlayers = 1;
    /// Its UDP port.
    std::uint16_t port = 0;
    /// The players the master added to it, sorted by id.
    std::vector<AdminGamePlayer> players;
    std::chrono::system_clock::time_point createdAt;
    /// Its last GAME_STATUS, nothing before the first.
    std::optional<GameStatusMessage> status;
    /// How long ago that status came, or the game got ready.
    std::chrono::milliseconds statusAge{0};
    /// No status for too long: its instance is stuck.
    bool stale = false;

    friend bool operator==(const AdminGame &, const AdminGame &) = default;
};

/// Why a running game ended.
enum class GameEnd : std::uint8_t {
    /// Its last player left.
    Empty,
    /// It was over: GAME_OVER.
    Over,
    /// Its instance crashed, or broke the protocol.
    Crashed,
    /// An administrator ended it.
    EndedByAdmin,
    /// The master stops.
    MasterStopping,
};

/// The games, for the administration. On the thread of the loop.
class AdminGames
{
public:
    AdminGames() = default;
    virtual ~AdminGames() = default;
    AdminGames(const AdminGames &) = delete;
    AdminGames &operator=(const AdminGames &) = delete;
    AdminGames(AdminGames &&) = delete;
    AdminGames &operator=(AdminGames &&) = delete;

    /// The running games, sorted by id.
    [[nodiscard]] virtual std::vector<AdminGame> adminGames() const = 0;
    /// Removes a player from its game: its instance gets REMOVE_PLAYER (KICKED), the player LEFT_GAME
    /// (KICKED).
    /// @return False if it is in no running game.
    virtual bool kick(std::uint32_t playerId) = 0;
    /// Ends a running game: its instance gets SHUTDOWN (ENDED_BY_ADMIN), its players LEFT_GAME
    /// (ENDED_BY_ADMIN).
    /// @return False if no running game has this id.
    virtual bool end(std::uint32_t gameId) = 0;
};

/// The lobby, for the administration. On the thread of the loop.
class AdminLobby
{
public:
    AdminLobby() = default;
    virtual ~AdminLobby() = default;
    AdminLobby(const AdminLobby &) = delete;
    AdminLobby &operator=(const AdminLobby &) = delete;
    AdminLobby(AdminLobby &&) = delete;
    AdminLobby &operator=(AdminLobby &&) = delete;

    /// The welcomed players, sorted by id.
    [[nodiscard]] virtual std::vector<AdminPlayer> adminPlayers() const = 0;
    /// Sends ERROR (`code`, KICKED or BANNED) to a player, then closes its connection, which takes it out of
    /// its game.
    /// @return False if no welcomed player has this id.
    virtual bool disconnect(std::uint32_t playerId, rtype::lobby::ErrorCode code) = 0;
    /// Sends SERVER_MESSAGE to a player. The text must be valid (rtype::lobby::isValidServerMessage()).
    /// @return False if no welcomed player has this id.
    virtual bool sendMessage(std::uint32_t playerId, const std::string &text) = 0;
};

/// What the lobby and the games tell the administration, as it happens, on the thread of the loop. Each call
/// gives an id: the administration reads the rest through AdminGames and AdminLobby, which are already up to
/// date.
class AdminObserver
{
public:
    AdminObserver() = default;
    virtual ~AdminObserver() = default;
    AdminObserver(const AdminObserver &) = delete;
    AdminObserver &operator=(const AdminObserver &) = delete;
    AdminObserver(AdminObserver &&) = delete;
    AdminObserver &operator=(AdminObserver &&) = delete;

    /// A client said HELLO, and was welcomed.
    virtual void playerJoined(std::uint32_t playerId) = 0;
    /// A player entered or left a game.
    virtual void playerChanged(std::uint32_t playerId) = 0;
    /// A player left the lobby: its connection closed.
    virtual void playerLeft(std::uint32_t playerId) = 0;
    /// A game is ready, and listed.
    virtual void gameStarted(std::uint32_t gameId) = 0;
    /// A running game changed: a player came or went, or its instance sent its status.
    virtual void gameChanged(std::uint32_t gameId) = 0;
    /// A running game ended.
    virtual void gameEnded(std::uint32_t gameId, GameEnd reason) = 0;
};

} // namespace rtype::server
