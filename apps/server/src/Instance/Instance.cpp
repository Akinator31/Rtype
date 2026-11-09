#include "Instance/Instance.hpp"

#include "Common/GameConfig.hpp"
#include "Common/InstanceMessages.hpp"
#include "Common/Lifecycle.hpp"
#include "Common/Secret.hpp"
#include "Instance/DirectTransport.hpp"
#include "Instance/GameHost.hpp"
#include "Instance/GameThread.hpp"
#include "Instance/GameTransport.hpp"
#include "Instance/MasterLink.hpp"
#include "Instance/RelayedTransport.hpp"
#include "rtype/game/Standalone.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/Session.hpp"
#include "rtype/network/Timer.hpp"

#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace rtype::server
{

namespace
{

using rtype::network::DisconnectReason;

/// How long the last datagrams and frames have to leave before the loop stops.
constexpr std::chrono::milliseconds Linger{100};
/// After the game over, how long the clients have to acknowledge GAME_OVER (docs/protocol/game.md,
/// GAME_OVER).
constexpr std::chrono::seconds GameOverWait{2};
/// How often the acknowledgements are checked meanwhile.
constexpr std::chrono::milliseconds GameOverCheck{50};
/// How often the master is told where the game stands (GAME_STATUS, docs/protocol/instances.md).
constexpr std::chrono::seconds StatusInterval{1};

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("instance");
    return instance;
}

/// Why the master shuts the game down, for the logs.
std::string_view describe(ShutdownReason reason)
{
    switch (reason) {
    case ShutdownReason::Empty:
        return "no player left";
    case ShutdownReason::MasterStopping:
        return "the master stops";
    case ShutdownReason::EndedByAdmin:
        return "an administrator ended the game";
    }
    return "unknown reason";
}

/// The player of a slot in standalone mode, with the public keys of rtype/game/Standalone.hpp.
AddPlayerMessage standalonePlayer(std::uint8_t slot)
{
    return {
        .playerId = static_cast<std::uint32_t>(slot) + 1,
        .sessionId = rtype::game::standalone::sessionId(slot),
        .clientKey = rtype::game::standalone::clientKey(slot),
        .serverKey = rtype::game::standalone::serverKey(slot),
        .name = std::format("Player {}", slot + 1),
    };
}

/// One game: its simulation thread, its network side, and its link to the master. Lives on the network
/// thread, the one that runs the loop.
class Instance
{
    const InstanceOptions &_options;
    rtype::network::EventLoop &_loop;
    GameThread _game;
    std::optional<GameHost> _host;
    std::optional<MasterLink> _link;
    rtype::network::Timer _linger;
    rtype::network::Timer _gameOverWait;
    rtype::network::Timer _status;
    int _exitCode = Success;
    bool _stopping = false;

    /// Opens the UDP port of the game, or its port on the loopback behind the relay of the master on
    /// `127.0.0.1:relayPort` (0: no relay), and starts the simulation.
    bool openGame(std::uint8_t maxPlayers, std::uint16_t relayPort, GameHost::Handlers handlers,
                  VoiceRules voice = {})
    {
        try {
            std::unique_ptr<GameTransport> transport;
            if (relayPort != 0) {
                transport = std::make_unique<RelayedTransport>(_loop, relayPort);
            } else {
                transport = std::make_unique<DirectTransport>(
                    _loop, rtype::network::Address::anyIpv4(_options.udpPort));
            }
            _host.emplace(_loop, std::move(transport), maxPlayers, _game, std::move(handlers), voice);
        } catch (const std::system_error &error) {
            if (relayPort != 0) {
                logger().error("Cannot open a UDP port on the loopback for the relay: {}",
                               error.code().message());
            } else {
                logger().error("Cannot open UDP port {}: {}", _options.udpPort, error.code().message());
            }
            return false;
        }
        _game.start();
        if (relayPort != 0) {
            logger().info("Game {} behind the relay on 127.0.0.1:{}, receiving on {}, {} players at most",
                          _options.id, relayPort, _host->localAddress(), maxPlayers);
        } else {
            logger().info("Game {} on UDP {}, {} players at most", _options.id, _host->localAddress(),
                          maxPlayers);
        }
        return true;
    }

    /// The game is over: the simulation stops, the clients have a moment to acknowledge GAME_OVER, then the
    /// master gets the scores, the sessions close with DISCONNECT (GAME_OVER), and the instance exits with 0.
    void gameOver(const GameOverReport &report)
    {
        _game.stop();
        _status.cancel();
        const auto deadline = std::chrono::steady_clock::now() + GameOverWait;
        _gameOverWait.startRepeating(GameOverCheck, [this, report, deadline] {
            if (!_host->acknowledged() && std::chrono::steady_clock::now() < deadline) {
                return;
            }
            _gameOverWait.cancel();
            if (_link) {
                _link->sendGameOver(report);
            }
            stop(Success, DisconnectReason::GameOver);
        });
    }

    void configure(const ConfigureMessage &game)
    {
        logger().info("Configured as \"{}\"", game.name);
        const bool opened =
            openGame(game.maxPlayers, game.relayPort,
                     {
                         .onConnected = [this](std::uint32_t player) { _link->sendPlayerConnected(player); },
                         .onDisconnected =
                             [this](std::uint32_t player, PlayerDisconnectedReason reason) {
                                 _link->sendPlayerDisconnected(player, reason);
                             },
                         .onGameOver = [this](const GameOverReport &report) { gameOver(report); },
                     },
                     game.voice);
        if (!game.voice.enabled) {
            logger().info("No voice in this game: the server turned it off");
        }
        if (!opened) {
            stop(Failure, DisconnectReason::Shutdown);
            return;
        }
        // The public port, or the one on the loopback where the relay sends its frames
        _link->sendReady(_host->localAddress().port());
        _status.startRepeating(StatusInterval, [this] { _link->sendGameStatus(_host->status()); });
    }

public:
    Instance(const InstanceOptions &options, rtype::game::GameConfig config, rtype::network::EventLoop &loop)
        : _options(options), _loop(loop),
          _game(
              {}, std::move(config),
              // Each tick, from the game thread to the network thread
              [this](TickResult result) {
                  _loop.post([this, result = std::move(result)] {
                      if (_host && !_stopping) {
                          _host->publish(result);
                      }
                  });
              },
              [this] { _loop.post([this] { stop(Failure, DisconnectReason::Shutdown); }); }),
          _linger(loop), _gameOverWait(loop), _status(loop)
    {
    }

    ~Instance() { _game.stop(); }

    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;

    [[nodiscard]] int exitCode() const { return _exitCode; }

    /// A game without a master: four sessions with the public keys, given again each time one closes.
    bool startStandalone()
    {
        const bool opened = openGame(
            MaxInstancePlayers, 0,
            {
                .onConnected = {},
                .onDisconnected =
                    [this](std::uint32_t player, PlayerDisconnectedReason) {
                        if (!_host->over()) {
                            _host->addPlayer(standalonePlayer(static_cast<std::uint8_t>(player - 1)));
                        }
                    },
                .onGameOver = [this](const GameOverReport &report) { gameOver(report); },
            });
        if (!opened) {
            return false;
        }
        for (std::uint8_t slot = 0; slot < MaxInstancePlayers; ++slot) {
            _host->addPlayer(standalonePlayer(slot));
        }
        return true;
    }

    /// Connects to the master, which configures the game, then gives its players.
    void startWithMaster(const Secret &secret)
    {
        _link.emplace(_loop, _options.masterPort, InstanceHelloMessage{.id = _options.id, .secret = secret},
                      MasterLink::Handlers{
                          .onConfigure = [this](const ConfigureMessage &game) { configure(game); },
                          .onAddPlayer =
                              [this](const AddPlayerMessage &player) {
                                  if (!_host->addPlayer(player)) {
                                      logger().error("The master added player {} to a full game, or twice",
                                                     player.playerId);
                                      stop(Failure, DisconnectReason::Shutdown);
                                  }
                              },
                          .onRemovePlayer =
                              [this](const RemovePlayerMessage &player) {
                                  if (!_host->removePlayer(player.playerId, player.reason)) {
                                      logger().debug("Player {} removed by the master was already gone",
                                                     player.playerId);
                                  }
                              },
                          .onMuteVoice =
                              [this](const MuteVoiceMessage &mute) {
                                  if (!_host->muteVoice(mute.playerId, mute.duration)) {
                                      logger().debug("Player {} muted by the master was already gone",
                                                     mute.playerId);
                                  }
                              },
                          .onShutdown =
                              [this](const ShutdownMessage &shutdown) {
                                  logger().info("Shut down by the master: {}", describe(shutdown.reason));
                                  stop(Success, DisconnectReason::Shutdown);
                              },
                          .onFailed =
                              [this](const std::string &reason) {
                                  logger().error("Instance {}: {}", _options.id, reason);
                                  stop(Failure, DisconnectReason::Shutdown);
                              },
                      });
        logger().info("Instance {} started: connecting to the master on 127.0.0.1:{}", _options.id,
                      _options.masterPort);
    }

    /// Ends the game: the clients receive `DISCONNECT`, the link closes, and the loop stops once the last
    /// datagrams are sent.
    void stop(int exitCode, DisconnectReason reason)
    {
        if (_stopping) {
            return;
        }
        _stopping = true;
        _exitCode = exitCode;
        _gameOverWait.cancel();
        _status.cancel();
        _game.stop();
        if (_host) {
            _host->closeAll(reason);
        }
        if (_link) {
            _link->close();
        }
        _linger.start(Linger, [this] { _loop.stop(); });
    }
};

} // namespace

int runInstance(const InstanceOptions &options, std::istream &input)
{
    std::optional<rtype::game::GameConfig> config =
        readGameConfig(getConfigDirectory(options.config), logger());
    if (!config) {
        return Failure;
    }
    rtype::network::EventLoop loop;
    Instance instance(options, std::move(*config), loop);

    if (options.standalone) {
        logger().warning("Standalone game {}: no master, public test keys. For development only", options.id);
        if (!instance.startStandalone()) {
            return Failure;
        }
    } else {
        logger().debug("Instance {} waiting for its secret on the standard input", options.id);
        const std::optional<Secret> secret = readSecret(input);
        if (!secret) {
            logger().error(
                "Instance {}: no secret on the standard input. An instance is started by the master",
                options.id);
            return Failure;
        }
        instance.startWithMaster(*secret);
    }

    const bool clean = runUntilStopped(loop, [&] { instance.stop(Success, DisconnectReason::Shutdown); });
    logger().info("Instance {} stopped", options.id);
    return clean ? instance.exitCode() : Failure;
}

} // namespace rtype::server
