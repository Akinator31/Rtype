#include "Master/Master.hpp"

#include "Common/GameConfig.hpp"
#include "Common/Lifecycle.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminStore.hpp"
#include "Master/Administration.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/Process.hpp"
#include "Master/ServerDatabase.hpp"
#include "Master/ServerKey.hpp"
#include "Web/LocalAccess.hpp"
#include "Web/WebServer.hpp"
#include "rtype/database/DatabaseThread.hpp"
#include "rtype/log/Log.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
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

} // namespace

int runMaster(const MasterOptions &options)
{
    const rtype::log::Logger logger("master");

    // Everything that can fail is done before running: a server that cannot work stops at once, with the
    // reason
    const std::optional<ServerKey> key = loadOrCreateKey(options.key, logger);
    if (!key) {
        return Failure;
    }
    const rtype::network::ServerIdentity &identity = key->identity;
    // The instances load it again: checked here, a server with a broken configuration does not start
    const std::filesystem::path config = std::filesystem::absolute(getConfigDirectory(options.config));
    if (!readGameConfig(config, logger)) {
        return Failure;
    }
    const std::optional<rtype::database::Address> databaseAt = databaseAddress(options, logger);
    if (!databaseAt) {
        return Failure;
    }
    std::optional<rtype::database::Database> database = openServerDatabase(*databaseAt, logger);
    if (!database) {
        return Failure;
    }
    // The bans of addresses outlive a restart (docs/architecture/administration.md#bans)
    std::vector<AddressBan> loadedBans;
    try {
        loadedBans = loadAddressBans(*database, std::chrono::system_clock::now());
    } catch (const soci::soci_error &error) {
        logger.error("Cannot read the bans of addresses: {}", error.what());
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

    // The panel (docs/architecture/web.md): declared after the loop, it stops before it
    const WebSettings &webSettings = options.settings.web;
    if (!options.settings.accountsEnabled() && !isLoopback(webSettings.listen)) {
        // Without accounts, nobody signs in to the panel: only this machine may reach it
        logger.error("The panel cannot listen on {} without accounts: only on a loopback address (127.0.0.1, "
                     "[::1])",
                     webSettings.listen);
        return Failure;
    }
    std::filesystem::path panelDir = webSettings.panelDir;
    if (!panelDir.empty() && !std::filesystem::is_directory(panelDir)) {
        logger.error("The folder of the panel {} does not exist", panelDir.string());
        return Failure;
    }
    const Secret accessToken = newAccessToken();
    std::optional<WebServer> web;
    try {
        if (panelDir.empty()) {
            panelDir = currentExecutable().parent_path() / "admin";
        }
        web.emplace(loop, WebServerSettings{
                              .listen = webSettings.listen,
                              .panelDir = panelDir,
                              .trustedProxies = webSettings.trustedProxies,
                              .publicUrl = webSettings.publicUrl,
                              .accessToken = accessToken,
                          });
    } catch (const std::system_error &error) {
        logger.error("Cannot serve the panel on {}: {}", webSettings.listen, error.code().message());
        return Failure;
    }

    const unsigned maxGames = options.lastGamePort - options.firstGamePort + 1U;
    logger.info("Lobby on TCP {}, games on UDP {}-{} ({} at most)", listener->localAddress(),
                options.firstGamePort, options.lastGamePort, maxGames);
    logger.info("Panel on HTTP {}", web->localAddress());
    if (!std::filesystem::is_regular_file(panelDir / "index.html")) {
        logger.info("The panel is not built: no index.html in {}", panelDir.string());
    }
    logger.info("Server key {}", identity.fingerprint().toHex());
    if (options.check) {
        logger.info("Check passed: the server can start");
        return Success;
    }

    // A new token at every start, never logged: whoever reads the file controls the server already
    const std::filesystem::path accessFile = options.key.parent_path() / AdminUrlFileName;
    const std::string panel = panelAddress(web->localAddress(), webSettings.publicUrl);
    try {
        writeAccessUrl(accessFile, accessUrl(panel, accessToken));
    } catch (const std::filesystem::filesystem_error &error) {
        logger.error("Cannot write {}: {}", accessFile.string(), error.code().message());
        return Failure;
    }
    logger.info("Panel at {}: open the address written in {}", panel, accessFile.string());

    // Its queries run on a thread of their own, their results come back on the loop. Declared after the loop,
    // it is destroyed before it: the tasks given until then are done (docs/architecture/database.md)
    rtype::database::DatabaseThread databaseThread(
        std::move(*database), [&loop](std::function<void()> function) { loop.post(std::move(function)); });

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
    AddressBans bans(loadedBans);
    if (!loadedBans.empty()) {
        logger.info("{} addresses banned", loadedBans.size());
    }
    Lobby lobby(loop, std::move(*listener), identity, *games);
    lobby.setBans(&bans);
    // The administration API (docs/architecture/administration.md), shown by the panel
    Administration administration(loop, *games, lobby, bans, &databaseThread,
                                  AdministrationSettings{
                                      .version = RTYPE_VERSION,
                                      .lobbyPort = lobby.localAddress().port(),
                                      .firstGamePort = options.firstGamePort,
                                      .lastGamePort = options.lastGamePort,
                                      .panelPort = web->localAddress().port(),
                                  });
    games->setObserver(&administration);
    lobby.setObserver(&administration);
    web->setAdministration(&administration);
    // Until every instance has exited, then the time for the last frames to leave
    rtype::network::Timer stopping(loop);
    logger.info("Master started: Ctrl+C to stop");
    const bool clean = runUntilStopped(loop, [&] {
        web->stop();
        lobby.close();
        games->shutdown();
        stopping.startRepeating(std::chrono::milliseconds(100), [&] {
            if (games->empty()) {
                stopping.cancel();
                loop.stop();
            }
        });
    });
    // The administration goes first: nothing tells it anything any more
    web->setAdministration(nullptr);
    lobby.setObserver(nullptr);
    games->setObserver(nullptr);
    logger.info("Master stopped");
    return clean ? Success : Failure;
}

int showFingerprint(const MasterOptions &options, std::ostream &out)
{
    const rtype::log::Logger logger("master");
    try {
        out << rtype::network::ServerIdentity::load(options.key).fingerprint().toHex() << '\n';
        return Success;
    } catch (const std::system_error &error) {
        logger.error("Cannot read the key {}: {}. The server generates it at its first start",
                     options.key.string(), error.code().message());
    } catch (const std::invalid_argument &error) {
        logger.error("{} is not a key of the server: {}", options.key.string(), error.what());
    }
    return Failure;
}

} // namespace rtype::server
