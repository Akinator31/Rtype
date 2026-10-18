#include "Master/Master.hpp"

#include "Common/Lifecycle.hpp"
#include "Master/Lobby.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/TcpListener.hpp"
#include "rtype/network/Timer.hpp"
#include "rtype/network/Tls.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <system_error>

namespace rtype::server
{

int runMaster(const MasterOptions &options)
{
    const rtype::log::Logger logger("master");

    // Everything that can fail is done before running: a server that cannot work stops at once, with the
    // reason
    std::optional<rtype::network::ServerIdentity> identity;
    try {
        identity = rtype::network::ServerIdentity::load(options.key);
    } catch (const std::system_error &error) {
        logger.error("Cannot read the key {}: {}. Make one with r-type_keygen, or give its path with --key",
                     options.key.string(), error.code().message());
        return Failure;
    } catch (const std::invalid_argument &error) {
        logger.error("{} is not a key of the server: {}", options.key.string(), error.what());
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

    const unsigned games = options.lastGamePort - options.firstGamePort + 1U;
    logger.info("Lobby on TCP {}, games on UDP {}-{} ({} at most)", listener->localAddress(),
                options.firstGamePort, options.lastGamePort, games);
    logger.info("Server key {}", identity->fingerprint().toHex());
    if (options.check) {
        logger.info("Check passed: the server can start");
        return Success;
    }

    Lobby lobby(loop, std::move(*listener), *identity);
    // How long the last frames have to leave before the loop stops
    rtype::network::Timer linger(loop);
    logger.info("Master started: Ctrl+C to stop");
    const bool clean = runUntilStopped(loop, [&] {
        lobby.close();
        linger.start(std::chrono::milliseconds(100), [&] { loop.stop(); });
    });
    logger.info("Master stopped");
    return clean ? Success : Failure;
}

} // namespace rtype::server
