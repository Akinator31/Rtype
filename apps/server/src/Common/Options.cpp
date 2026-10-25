#include "Common/Options.hpp"

#include <charconv>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace rtype::server
{

namespace
{

constexpr std::string_view Usage =
    R"(Usage: r-type_server [options]                  the master: the lobby, which starts the games
       r-type_server --instance [options]       one game, started by the master

Master options:
  -p, --port <port>          TCP port of the lobby (default: 4242; 0: any free port)
      --bind <ip>            Address to listen on (default: 0.0.0.0, every interface)
      --game-ports <a>-<b>   UDP ports of the games, one per game (default: 4243-4299)
  -k, --key <file>           Private key of the server (default: server.key), made with r-type_keygen
                             (debug builds without one: the public development key)
      --check                Check that the server can start (key, port, configuration), then exit

Instance options (given by the master):
      --id <id>              Id of the game, from 1
      --udp-port <port>      UDP port of the game
      --master-port <port>   Port of the master, on 127.0.0.1
      --standalone           A game without a master, with a test key (debug builds only)
  The master writes the secret of the instance on its standard input.

Other options:
      --config <dir>         Configuration of the game, the Lua files of the enemies and weapons
                             (default: the config folder next to the program)
      --log-level <level>    trace, debug, info, warning or error (default: RTYPE_LOG_LEVEL, or info)
  -h, --help                 Show this help
      --version              Show the version

Stop the server with Ctrl+C or SIGTERM; a second one stops it at once.
Exit codes: 0 when stopped normally, 1 on an error, 2 if the arguments are invalid.
)";

template <typename Number>
Number parseNumber(std::string_view option, std::string_view text, Number min, Number max)
{
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size() || value < min || value > max) {
        throw std::invalid_argument(
            std::format("{} expects a number from {} to {}, not '{}'", option, min, max, text));
    }
    return static_cast<Number>(value);
}

std::uint16_t parsePort(std::string_view option, std::string_view text)
{
    return parseNumber<std::uint16_t>(option, text, 1, std::numeric_limits<std::uint16_t>::max());
}

} // namespace

bool standaloneAvailable()
{
#ifdef RTYPE_STANDALONE_INSTANCE
    return true;
#else
    return false;
#endif
}

std::string_view usage()
{
    return Usage;
}

Options parseOptions(std::span<const std::string_view> arguments)
{
    Options options;
    // --help and --version win over everything else, even invalid arguments
    for (const std::string_view argument : arguments) {
        if (argument == "-h" || argument == "--help") {
            options.mode = Options::Mode::Help;
            return options;
        }
        if (argument == "--version") {
            options.mode = Options::Mode::Version;
            return options;
        }
    }

    std::vector<std::string_view> masterOnly;
    std::vector<std::string_view> instanceOnly;
    bool instance = false;
    std::optional<std::uint16_t> port;
    std::optional<std::string_view> bind;
    bool idGiven = false;
    bool udpPortGiven = false;
    bool masterPortGiven = false;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view argument = arguments[i];
        const auto value = [&]() {
            if (i + 1 >= arguments.size() || arguments[i + 1].empty()) {
                throw std::invalid_argument(std::format("{} needs a value", argument));
            }
            return arguments[++i];
        };

        if (argument == "--instance") {
            instance = true;
        } else if (argument == "--log-level") {
            const std::string_view name = value();
            options.logLevel = rtype::log::parseLevel(name);
            if (!options.logLevel) {
                throw std::invalid_argument(
                    std::format("unknown log level '{}': trace, debug, info, warning or error", name));
            }
        } else if (argument == "--config") {
            options.master.config = std::filesystem::path(std::string(value()));
            options.instance.config = options.master.config;
        } else if (argument == "-p" || argument == "--port") {
            // 0 lets the system choose: for the tests and --check
            port =
                parseNumber<std::uint16_t>(argument, value(), 0, std::numeric_limits<std::uint16_t>::max());
            masterOnly.push_back(argument);
        } else if (argument == "--bind") {
            bind = value();
            masterOnly.push_back(argument);
        } else if (argument == "--game-ports") {
            const std::string_view range = value();
            const std::size_t dash = range.find('-');
            if (dash == std::string_view::npos) {
                throw std::invalid_argument(
                    std::format("--game-ports expects <first>-<last>, not '{}'", range));
            }
            options.master.firstGamePort = parsePort(argument, range.substr(0, dash));
            options.master.lastGamePort = parsePort(argument, range.substr(dash + 1));
            if (options.master.firstGamePort > options.master.lastGamePort) {
                throw std::invalid_argument(std::format("--game-ports: {} is above {}",
                                                        options.master.firstGamePort,
                                                        options.master.lastGamePort));
            }
            masterOnly.push_back(argument);
        } else if (argument == "-k" || argument == "--key") {
            options.master.key = std::filesystem::path(std::string(value()));
            options.master.keyGiven = true;
            masterOnly.push_back(argument);
        } else if (argument == "--check") {
            options.master.check = true;
            masterOnly.push_back(argument);
        } else if (argument == "--id") {
            options.instance.id =
                parseNumber<std::uint32_t>(argument, value(), 1, std::numeric_limits<std::uint32_t>::max());
            idGiven = true;
            instanceOnly.push_back(argument);
        } else if (argument == "--udp-port") {
            options.instance.udpPort = parsePort(argument, value());
            udpPortGiven = true;
            instanceOnly.push_back(argument);
        } else if (argument == "--master-port") {
            options.instance.masterPort = parsePort(argument, value());
            masterPortGiven = true;
            instanceOnly.push_back(argument);
        } else if (argument == "--standalone") {
            options.instance.standalone = true;
            instanceOnly.push_back(argument);
        } else {
            throw std::invalid_argument(std::format("unknown argument '{}'", argument));
        }
    }

    if (!instance) {
        if (!instanceOnly.empty()) {
            throw std::invalid_argument(
                std::format("{} is an option of an instance: add --instance", instanceOnly[0]));
        }
        if (bind) {
            const auto address =
                rtype::network::Address::parse(*bind, port.value_or(options.master.listen.port()));
            if (!address) {
                throw std::invalid_argument(std::format("--bind expects an IP address, not '{}'", *bind));
            }
            options.master.listen = *address;
        } else if (port) {
            options.master.listen = rtype::network::Address::anyIpv4(*port);
        }
        options.mode = Options::Mode::Master;
        return options;
    }

    if (!masterOnly.empty()) {
        throw std::invalid_argument(
            std::format("{} is an option of the master, not of an instance", masterOnly[0]));
    }
    if (options.instance.standalone) {
        if (!standaloneAvailable()) {
            throw std::invalid_argument("--standalone only exists in debug builds");
        }
        if (masterPortGiven) {
            throw std::invalid_argument("--standalone runs without a master: --master-port cannot be used");
        }
        if (!idGiven) {
            options.instance.id = 1;
        }
    } else {
        if (!idGiven) {
            throw std::invalid_argument("an instance needs --id");
        }
        if (!masterPortGiven) {
            throw std::invalid_argument("an instance needs --master-port");
        }
    }
    if (!udpPortGiven) {
        throw std::invalid_argument("an instance needs --udp-port");
    }
    options.mode = Options::Mode::Instance;
    return options;
}

} // namespace rtype::server
