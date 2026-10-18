#include "Master/Games.hpp"

#include "rtype/log/Logger.hpp"
#include "rtype/network/Address.hpp"
#include "rtype/network/Datagram.hpp"
#include "rtype/network/TcpConnection.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace rtype::server
{

namespace
{

using Clock = std::chrono::steady_clock;
using rtype::lobby::ErrorCode;
using rtype::lobby::LeftGameMessage;
using rtype::lobby::LeftGameReason;

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("games");
    return instance;
}

/// 32 bytes from the random generator of the cryptography: a key is exactly that.
std::array<std::byte, 32> randomBytes()
{
    return rtype::network::DatagramKey::generate().bytes();
}

} // namespace

bool sameSecret(const Secret &first, const Secret &second)
{
    std::byte difference{0};
    for (std::size_t i = 0; i < first.size(); ++i) {
        difference |= first[i] ^ second[i];
    }
    return difference == std::byte{0};
}

struct Games::Game {
    enum class State {
        /// Started, not ready yet: not listed.
        Starting,
        /// READY received: listed, players can join.
        Running,
        /// Shut down, over or failed: waiting for the process to exit.
        Ending,
    };

    struct Member {
        std::uint32_t sessionId = 0;
    };

    struct Creator {
        LobbyPlayer player;
        JoinCallback done;
    };

    std::uint32_t id = 0;
    std::string name;
    std::uint8_t maxPlayers = 1;
    std::uint16_t port = 0;
    Secret secret{};
    std::optional<Process> process;
    State state = State::Starting;
    /// The connection of the instance, once it presented itself.
    std::optional<std::uint64_t> connection;
    rtype::network::Timer startTimeout;
    Clock::time_point endingSince;
    std::map<std::uint32_t, Member> members;
    std::uint32_t lastSession = 0;
    std::optional<Creator> creator;

    explicit Game(rtype::network::EventLoop &loop) : startTimeout(loop) {}

    [[nodiscard]] std::string describe() const { return std::format("game {} \"{}\"", id, name); }
};

struct Games::InstanceConnection {
    rtype::network::FramedConnection connection;
    rtype::network::Timer helloTimeout;
    /// The game of the instance, once it presented itself.
    std::optional<std::uint32_t> gameId;
    bool closed = false;

    InstanceConnection(rtype::network::TcpConnection tcp, rtype::network::EventLoop &loop)
        : connection(std::move(tcp)), helloTimeout(loop)
    {
    }
};

Games::Games(rtype::network::EventLoop &loop, GamesSettings settings)
    : _loop(loop), _settings(std::move(settings)), _listener(loop, rtype::network::Address::loopbackIpv4(0)),
      _supervision(loop)
{
    if (_settings.firstPort > _settings.lastPort) {
        throw std::invalid_argument("The range of the game ports is empty");
    }
    for (std::uint32_t port = _settings.firstPort; port <= _settings.lastPort; ++port) {
        _freePorts.insert(static_cast<std::uint16_t>(port));
    }
    _listener.startAccepting(
        [this](rtype::network::TcpConnection connection) { acceptInstance(std::move(connection)); });
    _supervision.startRepeating(SupervisionInterval, [this] { supervise(); });
}

Games::~Games()
{
    _supervision.cancel();
    _listener.close();
    // The processes left are stopped by their destructors: no instance outlives its master
}

Games::Game *Games::find(std::uint32_t gameId) const
{
    const auto found = _games.find(gameId);
    return found == _games.end() ? nullptr : found->second.get();
}

std::optional<int> Games::processId(std::uint32_t gameId) const
{
    const Game *game = find(gameId);
    if (game == nullptr || !game->process) {
        return std::nullopt;
    }
    return game->process->pid();
}

std::vector<rtype::lobby::GameInfo> Games::list() const
{
    std::vector<rtype::lobby::GameInfo> games;
    for (const auto &[id, game] : _games) {
        if (game->state == Game::State::Running && games.size() < rtype::lobby::MaxGamesInList) {
            games.push_back(
                {id, game->name, static_cast<std::uint8_t>(game->members.size()), game->maxPlayers});
        }
    }
    return games;
}

// --- Creating and joining ---

void Games::create(const LobbyPlayer &player, const rtype::lobby::CreateGameMessage &request,
                   JoinCallback done)
{
    if (_stopping || _freePorts.empty()) {
        logger().info("{} cannot create \"{}\": no game port left", player.name, request.name);
        done(ErrorCode::ServerFull);
        return;
    }
    auto created = std::make_unique<Game>(_loop);
    Game &game = *created;
    game.id = ++_lastGameId;
    game.name = request.name;
    game.maxPlayers = request.maxPlayers;
    game.port = *_freePorts.begin();
    game.secret = randomBytes();

    std::vector<std::string> arguments = _settings.leadingArguments;
    for (std::string argument :
         {std::string("--instance"), std::string("--id"), std::to_string(game.id), std::string("--udp-port"),
          std::to_string(game.port), std::string("--master-port"), std::to_string(instancePort())}) {
        arguments.push_back(std::move(argument));
    }
    arguments.insert(arguments.end(), _settings.trailingArguments.begin(), _settings.trailingArguments.end());
    try {
        game.process.emplace(
            _settings.executable, arguments,
            ProcessOptions{.input = formatSecret(game.secret) + "\n", .stopGrace = std::chrono::seconds(1)});
    } catch (const std::system_error &error) {
        logger().error("Cannot start an instance for \"{}\": {}", request.name, error.what());
        done(ErrorCode::GameStartFailed);
        return;
    }

    _freePorts.erase(game.port);
    game.creator = Game::Creator{player, std::move(done)};
    _playerGames[player.id] = game.id;
    const std::uint32_t id = game.id;
    game.startTimeout.start(_settings.startTimeout, [this, id] {
        if (Game *starting = find(id); starting != nullptr && starting->state == Game::State::Starting) {
            fail(*starting, "no READY in time");
        }
    });
    logger().info("{} creates {} for {} players: instance {} on UDP {}", player.name, game.describe(),
                  game.maxPlayers, game.process->pid(), game.port);
    _games.emplace(id, std::move(created));
}

rtype::lobby::GameJoinedMessage Games::addPlayer(Game &game, const LobbyPlayer &player)
{
    // Never 0, never reused in this instance
    const std::uint32_t session = ++game.lastSession;
    const AddPlayerMessage added{player.id, session, randomBytes(), randomBytes(), player.name};
    // To the instance first: it expects the session before the client connects
    send(game, instance_message::AddPlayer, encode(added));
    game.members[player.id] = Game::Member{session};
    _playerGames[player.id] = game.id;
    logger().info("{} ({}) joins {} ({}/{})", player.name, player.id, game.describe(), game.members.size(),
                  game.maxPlayers);
    return {game.id, game.port, session, added.clientKey, added.serverKey};
}

Games::JoinResult Games::join(const LobbyPlayer &player, std::uint32_t gameId)
{
    Game *game = find(gameId);
    if (game == nullptr || game->state != Game::State::Running) {
        return ErrorCode::GameNotFound;
    }
    if (game->members.size() >= game->maxPlayers) {
        return ErrorCode::GameFull;
    }
    return addPlayer(*game, player);
}

void Games::removed(Game &game, std::uint32_t playerId)
{
    game.members.erase(playerId);
    _playerGames.erase(playerId);
    if (game.members.empty() && game.state == Game::State::Running) {
        logger().info("{} has no player left", game.describe());
        shutDown(game, ShutdownReason::Empty);
    }
}

std::optional<std::uint32_t> Games::leave(std::uint32_t playerId)
{
    const auto found = _playerGames.find(playerId);
    if (found == _playerGames.end()) {
        return std::nullopt;
    }
    Game *game = find(found->second);
    if (game == nullptr || !game->members.contains(playerId)) {
        return std::nullopt;
    }
    send(*game, instance_message::RemovePlayer,
         encode(RemovePlayerMessage{playerId, RemovePlayerReason::Left}));
    const std::uint32_t gameId = game->id;
    removed(*game, playerId);
    return gameId;
}

void Games::lobbyClosed(std::uint32_t playerId)
{
    const auto found = _playerGames.find(playerId);
    if (found == _playerGames.end()) {
        return;
    }
    Game *game = find(found->second);
    if (game == nullptr) {
        _playerGames.erase(found);
        return;
    }
    if (game->creator && game->creator->player.id == playerId) {
        // Starting without its creator: shut down once ready, with no player
        game->creator.reset();
        _playerGames.erase(found);
        return;
    }
    if (game->members.contains(playerId)) {
        send(*game, instance_message::RemovePlayer,
             encode(RemovePlayerMessage{playerId, RemovePlayerReason::LobbyClosed}));
        removed(*game, playerId);
    }
}

// --- The instances ---

void Games::acceptInstance(rtype::network::TcpConnection tcp)
{
    const std::uint64_t id = ++_lastConnection;
    auto created = std::make_unique<InstanceConnection>(std::move(tcp), _loop);
    InstanceConnection &added = *created;
    _connections.emplace(id, std::move(created));
    added.helloTimeout.start(HelloTimeout, [this, id] {
        logger().warning("Instance connection {}: no INSTANCE_HELLO in time", id);
        closeInstanceConnection(id);
    });
    added.connection.start(
        [this, id](std::uint8_t type, std::span<const std::byte> payload) {
            instanceFrame(id, type, payload);
        },
        [this, id](std::error_code) { instanceClosed(id); });
}

void Games::closeInstanceConnection(std::uint64_t id)
{
    const auto found = _connections.find(id);
    if (found == _connections.end() || found->second->closed) {
        return;
    }
    found->second->closed = true;
    found->second->helloTimeout.cancel();
    found->second->connection.close();
    // Destroyed later: this may run in one of its handlers
    _loop.post([this, id, alive = std::weak_ptr<bool>(_alive)] {
        if (!alive.expired()) {
            _connections.erase(id);
        }
    });
}

void Games::instanceFrame(std::uint64_t connection, std::uint8_t type, std::span<const std::byte> payload)
{
    const auto found = _connections.find(connection);
    if (found == _connections.end() || found->second->closed) {
        return;
    }
    if (!found->second->gameId) {
        if (type != instance_message::InstanceHello) {
            logger().warning("Instance connection {}: message {:#04x} before INSTANCE_HELLO", connection,
                             type);
            closeInstanceConnection(connection);
            return;
        }
        presented(connection, payload);
        return;
    }
    Game *game = find(*found->second->gameId);
    if (game == nullptr) {
        closeInstanceConnection(connection);
        return;
    }
    switch (type) {
    case instance_message::Ready:
        ready(*game, payload);
        return;
    case instance_message::PlayerConnected:
        if (const auto player = decodePlayerConnected(payload)) {
            logger().debug("Player {} connected to {}", player->playerId, game->describe());
            return;
        }
        break;
    case instance_message::PlayerDisconnected:
        playerDisconnected(*game, payload);
        return;
    case instance_message::GameOver:
        gameOver(*game, payload);
        return;
    default:
        break;
    }
    fail(*game, std::format("unexpected or malformed message {:#04x} from its instance", type));
}

void Games::presented(std::uint64_t connection, std::span<const std::byte> payload)
{
    InstanceConnection &instance = *_connections.at(connection);
    const std::optional<InstanceHelloMessage> hello = decodeInstanceHello(payload);
    if (!hello || hello->version != InstanceProtocolVersion) {
        logger().warning("Instance connection {}: invalid INSTANCE_HELLO", connection);
        closeInstanceConnection(connection);
        return;
    }
    Game *game = find(hello->id);
    if (game == nullptr || game->state != Game::State::Starting || game->connection) {
        logger().warning("Instance connection {}: no instance {} expected", connection, hello->id);
        closeInstanceConnection(connection);
        return;
    }
    if (!sameSecret(hello->secret, game->secret)) {
        logger().warning("Instance connection {}: wrong secret for instance {}", connection, hello->id);
        closeInstanceConnection(connection);
        fail(*game, "an instance presented itself with a wrong secret");
        return;
    }
    instance.gameId = game->id;
    instance.helloTimeout.cancel();
    game->connection = connection;
    send(*game, instance_message::Configure, encode(ConfigureMessage{game->name, game->maxPlayers}));
}

void Games::ready(Game &game, std::span<const std::byte> payload)
{
    const std::optional<ReadyMessage> ready = decodeReady(payload);
    if (!ready || game.state != Game::State::Starting || ready->udpPort != game.port) {
        fail(game, "invalid READY from its instance");
        return;
    }
    game.state = Game::State::Running;
    game.startTimeout.cancel();
    logger().info("{} ready on UDP {}", game.describe(), game.port);
    if (!game.creator) {
        // Its creator left while it started
        shutDown(game, ShutdownReason::Empty);
        return;
    }
    Game::Creator creator = std::move(*game.creator);
    game.creator.reset();
    creator.done(addPlayer(game, creator.player));
}

void Games::playerDisconnected(Game &game, std::span<const std::byte> payload)
{
    const std::optional<PlayerDisconnectedMessage> player = decodePlayerDisconnected(payload);
    if (!player) {
        fail(game, "malformed PLAYER_DISCONNECTED from its instance");
        return;
    }
    if (!game.members.contains(player->playerId)) {
        // The master removed it already
        return;
    }
    const bool timedOut = player->reason == PlayerDisconnectedReason::TimedOut;
    const std::uint32_t gameId = game.id;
    removed(game, player->playerId);
    if (_onLeft) {
        _onLeft(player->playerId,
                timedOut ? std::optional(LeftGameMessage{gameId, LeftGameReason::TimedOut}) : std::nullopt);
    }
}

void Games::gameOver(Game &game, std::span<const std::byte> payload)
{
    const std::optional<GameOverReport> report = decodeGameOverReport(payload);
    if (!report) {
        fail(game, "malformed GAME_OVER from its instance");
        return;
    }
    logger().info("{} is over", game.describe());
    const std::map<std::uint32_t, Game::Member> members = std::move(game.members);
    game.members.clear();
    game.state = Game::State::Ending;
    game.endingSince = Clock::now();
    for (const auto &[playerId, member] : members) {
        _playerGames.erase(playerId);
        if (_onLeft) {
            _onLeft(playerId, LeftGameMessage{game.id, LeftGameReason::GameOver});
        }
    }
}

void Games::send(Game &game, std::uint8_t type, std::span<const std::byte> payload)
{
    if (!game.connection) {
        return;
    }
    const auto found = _connections.find(*game.connection);
    if (found != _connections.end() && !found->second->closed) {
        found->second->connection.send(type, payload);
    }
}

void Games::closeConnection(Game &game)
{
    if (game.connection) {
        closeInstanceConnection(*game.connection);
        game.connection.reset();
    }
}

void Games::shutDown(Game &game, ShutdownReason reason)
{
    if (game.state == Game::State::Ending) {
        return;
    }
    game.startTimeout.cancel();
    if (!game.connection) {
        // Not presented yet: nothing to tell it
        fail(game, "shut down before it started");
        return;
    }
    send(game, instance_message::Shutdown, encode(ShutdownMessage{reason}));
    game.state = Game::State::Ending;
    game.endingSince = Clock::now();
    for (const auto &[playerId, member] : game.members) {
        _playerGames.erase(playerId);
    }
    game.members.clear();
    if (game.creator) {
        // Still starting: its creator gets an answer all the same
        const Game::Creator creator = std::move(*game.creator);
        game.creator.reset();
        _playerGames.erase(creator.player.id);
        creator.done(ErrorCode::GameStartFailed);
    }
}

void Games::instanceClosed(std::uint64_t connection)
{
    const auto found = _connections.find(connection);
    if (found == _connections.end() || found->second->closed) {
        return;
    }
    const std::optional<std::uint32_t> gameId = found->second->gameId;
    closeInstanceConnection(connection);
    Game *game = gameId ? find(*gameId) : nullptr;
    if (game != nullptr) {
        game->connection.reset();
        if (game->state != Game::State::Ending) {
            fail(*game, "its instance closed the connection");
        }
    }
}

void Games::fail(Game &game, const std::string &reason)
{
    if (game.state == Game::State::Ending) {
        return;
    }
    const bool started = game.state == Game::State::Running;
    logger().error("{} {}: {}", game.describe(), started ? "crashed" : "failed to start", reason);
    game.state = Game::State::Ending;
    game.endingSince = Clock::now();
    game.startTimeout.cancel();
    closeConnection(game);
    if (game.process) {
        try {
            game.process->kill();
        } catch (const std::system_error &error) {
            logger().error("Cannot kill the instance of {}: {}", game.describe(), error.what());
        }
    }

    const std::optional<Game::Creator> creator = std::move(game.creator);
    game.creator.reset();
    const std::map<std::uint32_t, Game::Member> members = std::move(game.members);
    game.members.clear();
    if (creator) {
        _playerGames.erase(creator->player.id);
        creator->done(ErrorCode::GameStartFailed);
    }
    for (const auto &[playerId, member] : members) {
        _playerGames.erase(playerId);
        if (_onLeft) {
            _onLeft(playerId, LeftGameMessage{game.id, LeftGameReason::GameCrashed});
        }
    }
}

void Games::supervise()
{
    std::vector<std::uint32_t> exited;
    for (auto &[id, game] : _games) {
        std::optional<int> code;
        try {
            code = game->process->exitCode();
        } catch (const std::system_error &error) {
            logger().error("Cannot watch the instance of {}: {}", game->describe(), error.what());
            code = -1;
        }
        if (code) {
            if (game->state != Game::State::Ending) {
                fail(*game, std::format("its instance exited with code {}", *code));
            } else {
                logger().debug("Instance of {} exited with code {}", game->describe(), *code);
            }
            exited.push_back(id);
        } else if (game->state == Game::State::Ending &&
                   Clock::now() - game->endingSince > _settings.stopTimeout) {
            logger().warning("The instance of {} is still running: killing it", game->describe());
            game->endingSince = Clock::now();
            try {
                game->process->kill();
            } catch (const std::system_error &error) {
                logger().error("Cannot kill the instance of {}: {}", game->describe(), error.what());
            }
        }
    }
    for (const std::uint32_t id : exited) {
        Game &game = *_games.at(id);
        closeConnection(game);
        _freePorts.insert(game.port);
        logger().debug("{} ended: UDP {} free again", game.describe(), game.port);
        _games.erase(id);
    }
}

void Games::shutdown()
{
    if (_stopping) {
        return;
    }
    _stopping = true;
    _listener.close();
    if (!_games.empty()) {
        logger().info("Shutting down {} games", _games.size());
    }
    for (auto &[id, game] : _games) {
        shutDown(*game, ShutdownReason::MasterStopping);
    }
}

} // namespace rtype::server
