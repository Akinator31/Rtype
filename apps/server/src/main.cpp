#include "rtype/log/Logger.hpp"

int main()
{
    const rtype::log::Logger logger("server");

    if (const auto problem = rtype::log::configureFromEnvironment()) {
        logger.warning("{}", *problem);
    }
    logger.info("Server started");
    return 0;
}
