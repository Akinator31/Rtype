#pragma once

#include "rtype/log/Log.hpp"
#include "rtype/network/Address.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

/// The server, r-type_server: the master that runs the lobby, or with `--instance` one game started by it
/// (docs/architecture/server.md, ADR 0013).
namespace rtype::server
{

/// Exit codes of the program. The master reads those of its instances: anything but Success is a failure.
enum ExitCode : int {
    /// Stopped normally: Ctrl+C, SIGTERM, or the end of the game for an instance.
    Success = 0,
    /// Could not start (key, port...) or stopped on an error.
    Failure = 1,
    /// The arguments are invalid.
    InvalidArguments = 2,
};

/// The options of the master, the default mode.
struct MasterOptions {
    /// Where the master listens for the clients (TCP).
    rtype::network::Address listen = rtype::network::Address::anyIpv4(4242);
    /// The UDP ports given to the games, one per game: their number is the most games at once.
    std::uint16_t firstGamePort = 4243;
    std::uint16_t lastGamePort = 4299;
    /// The private key the master proves its identity with (docs/guides/generate-the-server-keys.md).
    std::filesystem::path key = "server.key";
    /// Only check that the server can start (key, ports, configuration), then exit.
    bool check = false;
    /// The configuration of the game, given to the instances; empty: config/ next to the executable.
    std::filesystem::path config{};
};

/// The options of an instance: one game, started by the master (docs/protocol/instances.md, section 1).
struct InstanceOptions {
    /// Id of the game, never 0.
    std::uint32_t id = 0;
    /// The UDP port of the game.
    std::uint16_t udpPort = 0;
    /// The port where the master listens for its instances, on 127.0.0.1.
    std::uint16_t masterPort = 0;
    /// A game without a master, with a fixed test key (debug builds only, ADR 0013).
    bool standalone = false;
    /// The configuration of the game; empty: config/ next to the executable.
    std::filesystem::path config{};
};

struct Options {
    enum class Mode {
        Master,
        Instance,
        /// --help: show the usage and exit.
        Help,
        /// --version: show the version and exit.
        Version,
    };

    Mode mode = Mode::Master;
    MasterOptions master;
    InstanceOptions instance;
    /// --log-level, which wins over RTYPE_LOG_LEVEL.
    std::optional<rtype::log::Level> logLevel;
};

/// Tells whether this build has the standalone mode (debug builds).
[[nodiscard]] bool standaloneAvailable();

/// The usage shown by --help.
[[nodiscard]] std::string_view usage();

/// Reads the arguments of the command line, without the name of the program, and checks them: the options of
/// one mode are refused in the other, and an instance needs its id and ports.
/// @throws std::invalid_argument with a message for the user.
[[nodiscard]] Options parseOptions(std::span<const std::string_view> arguments);

} // namespace rtype::server
