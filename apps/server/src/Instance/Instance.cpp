#include "Instance/Instance.hpp"

#include "Common/Lifecycle.hpp"
#include "Common/Secret.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/network/EventLoop.hpp"
#include "rtype/network/UdpSocket.hpp"

#include <optional>
#include <system_error>

namespace rtype::server
{

int runInstance(const InstanceOptions &options, std::istream &input)
{
    const rtype::log::Logger logger("instance");
    rtype::network::EventLoop loop;
    std::optional<rtype::network::UdpSocket> socket;

    if (options.standalone) {
        logger.warning("Standalone game {}: no master, a fixed test key. For development only", options.id);
        try {
            socket.emplace(loop, rtype::network::Address::anyIpv4(options.udpPort));
        } catch (const std::system_error &error) {
            logger.error("Cannot open UDP port {}: {}", options.udpPort, error.code().message());
            return Failure;
        }
        logger.info("Game {} on UDP {}", options.id, socket->localAddress());
    } else {
        logger.debug("Instance {} waiting for its secret on the standard input", options.id);
        if (!readSecret(input)) {
            logger.error("Instance {}: no secret on the standard input. An instance is started by the master",
                         options.id);
            return Failure;
        }
        // The connection to the master and the UDP port come with the instance mode
        logger.info("Instance {} started (UDP {}, master on 127.0.0.1:{})", options.id, options.udpPort,
                    options.masterPort);
    }

    const bool clean = runUntilStopped(loop, [&] {
        if (socket) {
            socket->close();
        }
        loop.stop();
    });
    logger.info("Instance {} stopped", options.id);
    return clean ? Success : Failure;
}

} // namespace rtype::server
