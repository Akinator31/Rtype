#include "Master/Master.hpp"

#include "Common/GameConfig.hpp"
#include "Common/Lifecycle.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/Process.hpp"
#include "rtype/keys/DevelopmentKey.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/Tls.hpp"

#ifdef RTYPE_DEVELOPMENT_KEY
#include "rtype/keys/DevelopmentPrivateKey.hpp"
#endif

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace rtype::server
{

namespace
{

/// The level of the logs of the master, for its instances: they log like it.
std::string levelArgument()
{
    switch (rtype::log::level()) {
    case rtype::log::Level::Trace:
        return "trace";
    case rtype::log::Level::Debug:
        return "debug";
    case rtype::log::Level::Info:
        return "info";
    case rtype::log::Level::Warning:
        return "warning";
    case rtype::log::Level::Error:
        return "error";
    }
    return "info";
}

/// The key of the master: the file of --key, or server.key. A Debug master without either uses the
/// development key; a Release master refuses it (ADR 0020).
std::optional<rtype::network::ServerIdentity> loadIdentity(const MasterOptions &options,
                                                           const rtype::log::Logger &logger)
{
#ifdef RTYPE_DEVELOPMENT_KEY
    if (!options.keyGiven && !std::filesystem::exists(options.key)) {
        logger.warning("No {}: the server uses the public development key, which only Debug clients trust",
                       options.key.string());
        return rtype::network::ServerIdentity::fromPrivateKey(rtype::keys::DevelopmentPrivateKey);
    }
#endif
    std::optional<rtype::network::ServerIdentity> identity;
    try {
        identity = rtype::network::ServerIdentity::load(options.key);
    } catch (const std::system_error &error) {
        logger.error("Cannot read the key {}: {}. Make one with r-type_keygen, or give its path with --key",
                     options.key.string(), error.code().message());
        return std::nullopt;
    } catch (const std::invalid_argument &error) {
        logger.error("{} is not a key of the server: {}", options.key.string(), error.what());
        return std::nullopt;
    }
    if (!rtype::keys::DevelopmentKeyTrusted &&
        identity->fingerprint() == rtype::keys::developmentFingerprint()) {
        logger.error("{} is the public development key: anyone could pretend to be this server. Make a key "
                     "of its own with r-type_keygen",
                     options.key.string());
        return std::nullopt;
    }
    return identity;
}

} // namespace

int runMaster(const MasterOptions &options)
{
    const rtype::log::Logger logger("master");

    // Everything that can fail is done before running: a server that cannot work stops at once, with the
    // reason
    const std::optional<rtype::network::ServerIdentity> identity = loadIdentity(options, logger);
    if (!identity) {
        return Failure;
    }
    // The instances load it again: checked here, a server with a broken configuration does not start
    const std::filesystem::path config = std::filesystem::absolute(getConfigDirectory(options.config));
    if (!readGameConfig(config, logger)) {
        return Failure;
    }

    rtype::network::EventLoop loop;
    std::optional<rtype::network::TcpListener> listener;
    try {
        listener.emplace(loop, options.listen);
    } catch (const std::system_error &error) {
        logger.error("Cannot listen on {}: {}", options.listen, error.code().message());
        return Failure;
    }

    const unsigned maxGames = options.lastGamePort - options.firstGamePort + 1U;
    logger.info("Lobby on TCP {}, games on UDP {}-{} ({} at most)", listener->localAddress(),
                options.firstGamePort, options.lastGamePort, maxGames);
    logger.info("Server key {}", identity->fingerprint().toHex());
    if (options.check) {
        logger.info("Check passed: the server can start");
        return Success;
    }

    std::optional<Games> games;
    try {
        games.emplace(loop,
                      GamesSettings{
                          .executable = currentExecutable(),
                          .leadingArguments = {},
                          .trailingArguments = {"--log-level", levelArgument(), "--config", config.string()},
                          .firstPort = options.firstGamePort,
                          .lastPort = options.lastGamePort,
                      });
    } catch (const std::system_error &error) {
        logger.error("Cannot prepare the games: {}", error.what());
        return Failure;
    }
    Lobby lobby(loop, std::move(*listener), *identity, *games);
    // Until every instance has exited, then the time for the last frames to leave
    rtype::network::Timer stopping(loop);
    logger.info("Master started: Ctrl+C to stop");
    const bool clean = runUntilStopped(loop, [&] {
        lobby.close();
        games->shutdown();
        stopping.startRepeating(std::chrono::milliseconds(100), [&] {
            if (games->empty()) {
                stopping.cancel();
                loop.stop();
            }
        });
    });
    logger.info("Master stopped");
    return clean ? Success : Failure;
}

} // namespace rtype::server
