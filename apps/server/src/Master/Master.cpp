#include "Master/Master.hpp"

#include "Accounts/AccountService.hpp"
#include "Accounts/CurlMailer.hpp"
#include "Accounts/Discord.hpp"
#include "Accounts/PanelAccounts.hpp"
#include "Accounts/PasswordHasher.hpp"
#include "Common/GameConfig.hpp"
#include "Common/Lifecycle.hpp"
#include "Master/AddressBans.hpp"
#include "Master/AdminStore.hpp"
#include "Master/Administration.hpp"
#include "Master/Games.hpp"
#include "Master/Lobby.hpp"
#include "Master/Process.hpp"
#include "Master/Relay.hpp"
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
    // The mutes of the voice of the accounts too (docs/architecture/administration.md#the-voice)
    std::vector<VoiceMute> loadedVoiceMutes;
    try {
        loadedVoiceMutes = loadVoiceMutes(*database, std::chrono::system_clock::now());
    } catch (const soci::soci_error &error) {
        logger.error("Cannot read the mutes of the voice: {}", error.what());
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
    // Every game on one UDP port (--game-port), on the address of the lobby, through the relay: opened here,
    // a port taken stops the master at start, as --check. It outlives the games, which tell it their sessions
    std::optional<Relay> relay;
    const rtype::network::Address relayAddress = options.listen.withPort(options.gamePort);
    try {
        relay.emplace(relayAddress, loadedBans);
    } catch (const std::system_error &error) {
        logger.error("Cannot open UDP {} for the games: {}", relayAddress, error.code().message());
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

    // A master with accounts sends their codes: a mail server it cannot use stops it at start, as --check
    // (docs/guides/set-up-accounts.md). Nothing is sent
    std::optional<CurlMailerSettings> mailerSettings;
    if (options.settings.accountsEnabled()) {
        if (!options.settings.mail) {
            logger.error("The accounts need a mail server, to send their codes: add [mail] to the settings");
            return Failure;
        }
        CurlMailerSettings &mailer = mailerSettings.emplace();
        mailer.mail = *options.settings.mail;
        const MailSettings &mail = mailer.mail;
        if (const std::optional<std::string> problem = checkMailServer(mailer)) {
            logger.error("Cannot use the mail server {}:{}: {}", mail.host, mail.port, *problem);
            return Failure;
        }
        logger.info("Mail server {}:{}, mails from {}", mail.host, mail.port, mail.from);
        if (mail.security == MailSecurity::None) {
            logger.warning("The mails go to {} in the clear (security = \"none\"): only for a relay on this "
                           "machine or a test server",
                           mail.host);
        }
    }

    // Discord too: an application it refuses stops the master at start, as --check
    std::optional<DiscordClientSettings> discordSettings;
    if (options.settings.accountsEnabled() && options.settings.discord) {
        discordSettings.emplace().discord = *options.settings.discord;
        if (const std::optional<std::string> problem = checkDiscord(*discordSettings)) {
            logger.error("Cannot use the Discord application {}: {}", discordSettings->discord.clientId,
                         *problem);
            return Failure;
        }
        logger.info("Discord application {}", discordSettings->discord.clientId);
    }

    logger.info("Lobby on TCP {}, games on UDP {} ({} at most)", listener->localAddress(),
                relay->publicAddress(), options.maxGames);
    const VoiceRules &voice = options.settings.voice;
    if (voice.enabled) {
        logger.info("Voice on: frames of {} bytes, {} per second at most", voice.maxFrameBytes,
                    voice.maxFramesPerSecond);
    } else {
        logger.info("Voice off: the games carry no voice ([voice] enabled = false)");
    }
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
        games.emplace(loop, *relay,
                      GamesSettings{
                          .executable = currentExecutable(),
                          .leadingArguments = {},
                          .trailingArguments = {"--log-level", levelArgument(), "--config", config.string()},
                          .maxGames = options.maxGames,
                          .voice = options.settings.voice,
                      });
    } catch (const std::system_error &error) {
        logger.error("Cannot prepare the games: {}", error.what());
        return Failure;
    }
    AddressBans bans(loadedBans);
    // The relay drops the datagrams of a banned address before any game sees them
    bans.setChangeHandler([&bans, &relay] { relay->setBans(bans.list()); });
    if (!loadedBans.empty()) {
        logger.info("{} addresses banned", loadedBans.size());
    }
    VoiceMutes voiceMutes(loadedVoiceMutes);
    games->setVoiceMutes(&voiceMutes);
    if (!loadedVoiceMutes.empty()) {
        logger.info("{} accounts muted", loadedVoiceMutes.size());
    }
    // The accounts of the players (docs/architecture/accounts.md#in-the-lobby): declared before the lobby,
    // they outlive it
    std::optional<CurlMailer> mailer;
    std::optional<PasswordHasher> hasher;
    std::optional<DiscordClient> discord;
    std::optional<AccountService> accounts;
    std::optional<PanelAccounts> panelAccounts;
    if (mailerSettings) {
        const auto post = [&loop](std::function<void()> function) { loop.post(std::move(function)); };
        mailer.emplace(*mailerSettings);
        hasher.emplace(post);
        if (discordSettings) {
            discord.emplace(*discordSettings, post);
        }
        accounts.emplace(loop, databaseThread, *hasher, *mailer,
                         AccountServiceSettings{.guests = options.settings.accounts->guests, .limits = {}},
                         discord ? &*discord : nullptr);
        logger.info("Accounts on, {}{}", options.settings.accounts->guests ? "guests allowed" : "no guests",
                    discord ? ", Discord sign-in" : "");
        // The administrators sign in to the panel with their account (docs/architecture/web.md#accounts)
        panelAccounts.emplace(loop, databaseThread, *hasher, discord ? &*discord : nullptr,
                              PanelAccountsSettings{.admins = options.settings.admin.admins,
                                                    .publicUrl = webSettings.publicUrl});
        if (discord && webSettings.publicUrl.empty()) {
            logger.info("No Discord sign-in for the panel: it needs web.public_url, where Discord sends the "
                        "administrators back");
        }
        panelAccounts->promoteListed([&logger](std::optional<std::size_t> promoted) {
            if (promoted && *promoted > 0) {
                logger.info("{} accounts of [admin] admins made administrators", *promoted);
            }
        });
    }
    Lobby lobby(loop, std::move(*listener), identity, *games);
    lobby.setBans(&bans);
    lobby.setAccounts(accounts ? &*accounts : nullptr);
    // The administration API (docs/architecture/administration.md), shown by the panel
    Administration administration(loop, *games, lobby, bans, &databaseThread,
                                  AdministrationSettings{
                                      .version = RTYPE_VERSION,
                                      .lobbyPort = lobby.localAddress().port(),
                                      .gamePort = relay->publicAddress().port(),
                                      .maxGames = options.maxGames,
                                      .panelPort = web->localAddress().port(),
                                      .admins = options.settings.admin.admins,
                                      .voice = options.settings.voice.enabled,
                                  },
                                  &voiceMutes);
    games->setObserver(&administration);
    lobby.setObserver(&administration);
    web->setAdministration(&administration);
    web->setPanelAccounts(panelAccounts ? &*panelAccounts : nullptr);
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
    // Once the instances are gone, their last DISCONNECT forwarded: before the loop of the master
    relay->stop();
    // The administration goes first: nothing tells it anything any more
    bans.setChangeHandler({});
    web->setAdministration(nullptr);
    web->setPanelAccounts(nullptr);
    lobby.setObserver(nullptr);
    games->setObserver(nullptr);
    games->setVoiceMutes(nullptr);
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
