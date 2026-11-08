#pragma once

#include "Common/InstanceMessages.hpp"
#include "Common/Secret.hpp"
#include "Master/AdminModel.hpp"
#include "Master/Process.hpp"
#include "rtype/lobby/Messages.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/FramedConnection.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace rtype::server
{

class Relay;

/// How the master runs its games.
struct GamesSettings {
    /// The program of the instances: the master itself (currentExecutable()), or another one in the tests.
    std::filesystem::path executable;
    /// Given before the arguments of the instance: for a program of the tests.
    std::vector<std::string> leadingArguments;
    /// Given after them, such as `--log-level debug`.
    std::vector<std::string> trailingArguments;
    /// The most games at once, running or ending: past it, SERVER_FULL.
    std::uint32_t maxGames = 57;
    /// From the start of an instance to its READY (docs/protocol/instances.md, section 3).
    std::chrono::milliseconds startTimeout{std::chrono::seconds(10)};
    /// From SHUTDOWN or GAME_OVER to the exit of an instance, before it is killed.
    std::chrono::milliseconds stopTimeout{std::chrono::seconds(5)};
    /// A running game without GAME_STATUS for this long is stale: its instance, or its game thread, is
    /// stuck. Its instances report every second.
    std::chrono::milliseconds statusTimeout{std::chrono::seconds(3)};
};

/// Where a game stands, as its instance last said (GAME_STATUS).
struct GameStatus {
    /// The last GAME_STATUS, nothing before the first.
    std::optional<GameStatusMessage> last;
    /// How long ago it came, or the game got ready if none came yet.
    std::chrono::steady_clock::duration age{};
    /// Nothing came for GamesSettings::statusTimeout.
    bool stale = false;
};

/// A player of the lobby, as the games know it.
struct LobbyPlayer {
    std::uint32_t id = 0;
    std::string name;
};

/// The games of the master (ADR 0013): one instance per game, started with Process, connected back on a local
/// listener, and watched until it exits (docs/protocol/instances.md). On the thread of the loop, used by the
/// lobby.
///
/// @code
/// rtype::server::Relay relay(rtype::network::Address::anyIpv4(4242));
/// rtype::server::Games games(loop, relay, {.executable = rtype::server::currentExecutable()});
/// games.create({playerId, "Pavel"}, {"Bydo hunters", 4}, [&](rtype::server::Games::JoinResult result) {
///     // GAME_JOINED, or the error to answer
/// });
/// @endcode
///
/// - **Creating** takes a place among GamesSettings::maxGames, draws a secret, starts the instance and waits
///   for its READY: only then is the creator added and answered. No place: SERVER_FULL. No READY in time, or
///   an instance that cannot start: GAME_START_FAILED, and the instance is killed.
/// - **Joining** adds a player to a game that is ready: it gets a session and its keys, given to the instance
///   (ADD_PLAYER) before the player (GAME_JOINED). A session id is random, never 0, and unique across the
///   whole master: it names one player of one game, and stays taken until the instance of its game exits, so
///   that an instance never sees the same id twice.
/// - **A game without players** is shut down (SHUTDOWN (EMPTY)).
/// - **An instance that dies**, or closes its connection, without GAME_OVER has crashed: its players are told
///   (LEFT_GAME (GAME_CRASHED)). A game over is told too (LEFT_GAME (GAME_OVER)).
/// - **Its status** (GAME_STATUS, every second) is kept with its time: status() says where it stands, and
///   whether it stopped reporting.
/// - **Its place is freed** once the process has exited, killed if it does not within the stop timeout.
/// - **Behind the relay** of the master, every game: the instance is given the relay in CONFIGURE, and the
///   relay is told where the instance receives (READY), each session added and released, and the instance
///   gone. GAME_JOINED gives the public port of the relay to every player.
/// - **The administration** sees the running games and acts on them through AdminGames, and is told what
///   happens to them through its AdminObserver (docs/architecture/administration.md).
class Games final : public AdminGames
{
public:
    using JoinResult = std::variant<rtype::lobby::GameJoinedMessage, rtype::lobby::ErrorCode>;
    using JoinCallback = std::function<void(JoinResult result)>;
    /// A player is no longer in its game without having asked: with the LEFT_GAME to send, or nothing when
    /// it closed its session itself (`PLAYER_DISCONNECTED (QUIT)`).
    using LeftHandler = std::function<void(std::uint32_t playerId,
                                           std::optional<rtype::lobby::LeftGameMessage> notification)>;
    /// A game is over: what each player connected at its end did, before they are told they left it.
    using GameOverHandler = std::function<void(const GameOverReport &report)>;
    /// Draws a candidate session id, refused if it is 0 or already taken.
    using SessionDraw = std::function<std::uint32_t()>;

    /// How often the processes are checked.
    static constexpr std::chrono::milliseconds SupervisionInterval{50};
    /// How long an instance has to present itself once connected.
    static constexpr std::chrono::seconds HelloTimeout{10};

private:
    struct Game;
    struct InstanceConnection;

    rtype::network::EventLoop &_loop;
    Relay &_relay;
    GamesSettings _settings;
    LeftHandler _onLeft;
    GameOverHandler _onGameOver;
    AdminObserver *_observer = nullptr;
    SessionDraw _drawSession;
    rtype::network::TcpListener _listener;
    rtype::network::Timer _supervision;
    std::map<std::uint32_t, std::unique_ptr<Game>> _games;
    std::map<std::uint64_t, std::unique_ptr<InstanceConnection>> _connections;
    /// The game of each player, creators waiting for theirs included.
    std::map<std::uint32_t, std::uint32_t> _playerGames;
    /// The session ids taken, in every game whose instance has not exited yet.
    std::set<std::uint32_t> _sessions;
    std::uint32_t _lastGameId = 0;
    std::uint64_t _lastConnection = 0;
    bool _stopping = false;
    std::shared_ptr<bool> _alive = std::make_shared<bool>(true);

    [[nodiscard]] Game *find(std::uint32_t gameId) const;
    void acceptInstance(rtype::network::TcpConnection connection);
    void instanceFrame(std::uint64_t connection, std::uint8_t type, std::span<const std::byte> payload);
    void instanceClosed(std::uint64_t connection);
    void presented(std::uint64_t connection, std::span<const std::byte> payload);
    void ready(Game &game, std::span<const std::byte> payload);
    void playerDisconnected(Game &game, std::span<const std::byte> payload);
    void gameOver(Game &game, std::span<const std::byte> payload);
    void statusReceived(Game &game, std::span<const std::byte> payload);
    /// A new session id, never 0, never one of _sessions, which it joins.
    [[nodiscard]] std::uint32_t takeSession();
    rtype::lobby::GameJoinedMessage addPlayer(Game &game, const LobbyPlayer &player);
    void removed(Game &game, std::uint32_t playerId);
    /// Sends SHUTDOWN, then waits for the exit.
    void shutDown(Game &game, ShutdownReason reason);
    /// A running game ends: the administration is told.
    void ended(const Game &game, GameEnd reason);
    void changed(const Game &game);
    /// The game failed or crashed: its players are told, its instance killed.
    void fail(Game &game, const std::string &reason);
    void closeConnection(Game &game);
    void send(Game &game, std::uint8_t type, std::span<const std::byte> payload);
    void supervise();
    void closeInstanceConnection(std::uint64_t id);

public:
    /// Listens for the instances on 127.0.0.1, on a port of the system. `relay` serves the games on its
    /// public port: it must outlive them.
    /// @throws std::system_error if it cannot listen.
    /// @throws std::invalid_argument if maxGames is 0.
    Games(rtype::network::EventLoop &loop, Relay &relay, GamesSettings settings);
    ~Games() override;

    Games(const Games &) = delete;
    Games &operator=(const Games &) = delete;
    Games(Games &&) = delete;
    Games &operator=(Games &&) = delete;

    /// Where the instances are told to connect.
    [[nodiscard]] std::uint16_t instancePort() const { return _listener.localAddress().port(); }

    /// Called when a player leaves its game without asking.
    void setLeftHandler(LeftHandler handler) { _onLeft = std::move(handler); }
    /// Called when an instance reports the end of its game.
    void setGameOverHandler(GameOverHandler handler) { _onGameOver = std::move(handler); }
    /// Tells the administration what happens to the games; nullptr for nobody. It must outlive the games, or
    /// be taken away first.
    void setObserver(AdminObserver *observer) { _observer = observer; }
    /// Replaces the random generator of the system for the session ids: for the tests, to force collisions.
    void setSessionDraw(SessionDraw draw) { _drawSession = std::move(draw); }

    /// The games ready to be joined, sorted by id: for GAME_LIST.
    [[nodiscard]] std::vector<rtype::lobby::GameInfo> list() const;

    /// CREATE_GAME: answered once the instance is ready, or failed.
    void create(const LobbyPlayer &player, const rtype::lobby::CreateGameMessage &game, JoinCallback done);
    /// JOIN_GAME.
    [[nodiscard]] JoinResult join(const LobbyPlayer &player, std::uint32_t gameId);
    /// LEAVE_GAME: the player leaves its game, which it is told by its instance.
    /// @return The game it left, or nothing if it was in none.
    std::optional<std::uint32_t> leave(std::uint32_t playerId);
    /// The lobby connection of a player closed: it leaves its game, and a game it was creating has no
    /// creator any more.
    void lobbyClosed(std::uint32_t playerId);

    /// The master stops: every game is shut down (SHUTDOWN (MASTER_STOPPING)).
    void shutdown();
    /// No instance runs any more.
    [[nodiscard]] bool empty() const { return _games.empty(); }
    /// Games, running or ending.
    [[nodiscard]] std::size_t count() const { return _games.size(); }

    /// The last status of a running game, nothing for a game unknown, starting or ending.
    [[nodiscard]] std::optional<GameStatus> status(std::uint32_t gameId) const;

    /// The process id of the instance of a game, for the tests and the logs.
    [[nodiscard]] std::optional<int> processId(std::uint32_t gameId) const;

    [[nodiscard]] std::vector<AdminGame> adminGames() const override;
    bool kick(std::uint32_t playerId) override;
    bool end(std::uint32_t gameId) override;
};

} // namespace rtype::server
