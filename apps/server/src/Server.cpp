#include "Server.hpp"

#include "Common/Options.hpp"
#include "Instance/Instance.hpp"
#include "Master/Master.hpp"
#include "rtype/log/Logger.hpp"

#include <exception>
#include <stdexcept>

#ifndef RTYPE_VERSION
#define RTYPE_VERSION "unknown"
#endif

namespace rtype::server
{

std::string_view version()
{
    return RTYPE_VERSION;
}

int run(std::span<const std::string_view> arguments, std::ostream &out, std::istream &in)
{
    const rtype::log::Logger logger("server");

    Options options;
    try {
        options = parseOptions(arguments);
    } catch (const std::invalid_argument &error) {
        logger.error("{} (see --help)", error.what());
        return InvalidArguments;
    }
    if (options.logLevel) {
        rtype::log::setLevel(*options.logLevel);
    }
    if (!options.master.settingsFile.empty() && options.mode != Options::Mode::Help &&
        options.mode != Options::Mode::Version) {
        // Read again over the settings file, for the command line to win over it
        try {
            options = parseOptions(arguments, loadSettings(options.master.settingsFile, logger));
        } catch (const SettingsError &error) {
            logger.error("{}", error.what());
            return Failure;
        } catch (const std::invalid_argument &error) {
            logger.error("{} (see --help)", error.what());
            return InvalidArguments;
        }
        logger.info("Settings {}", options.master.settingsFile.string());
    }

    try {
        switch (options.mode) {
        case Options::Mode::Help:
            out << usage();
            return Success;
        case Options::Mode::Version:
            out << "r-type_server " << version() << '\n';
            return Success;
        case Options::Mode::Fingerprint:
            return showFingerprint(options.master, out);
        case Options::Mode::Instance:
            return runInstance(options.instance, in);
        case Options::Mode::Master:
            break;
        }
        return runMaster(options.master);
    } catch (const std::exception &error) {
        // A bug, or a resource the system refuses: the server stops rather than run in an unknown state
        logger.error("Stopped on an unexpected error: {}", error.what());
        return Failure;
    }
}

} // namespace rtype::server
