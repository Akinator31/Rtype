#include "Common/Lifecycle.hpp"

#include "rtype/log/Logger.hpp"

namespace rtype::server
{

bool runUntilStopped(rtype::network::EventLoop &loop, std::function<void()> shutdown,
                     rtype::network::Timer::Clock::duration grace)
{
    const rtype::log::Logger logger("server");
    int signals = 0;
    bool forced = false;
    rtype::network::Timer deadline(loop);

    const rtype::network::StopSignals stop(loop, [&](const char *signal) {
        ++signals;
        if (signals > 1) {
            logger.warning("{} received again: stopping at once", signal);
            forced = true;
            loop.stop();
            return;
        }
        logger.info("{} received: stopping (again to stop at once)", signal);
        deadline.start(grace, [&] {
            logger.warning("Still stopping after {:.1f} s: stopping at once",
                           std::chrono::duration<double>(grace).count());
            forced = true;
            loop.stop();
        });
        if (shutdown) {
            shutdown();
        }
    });

    loop.run();
    deadline.cancel();
    return !forced;
}

} // namespace rtype::server
