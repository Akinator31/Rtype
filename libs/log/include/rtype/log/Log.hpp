#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

/// Logging shared by the client and the server: levels, output and global settings.
///
/// Messages are written through a Logger, which gives them the name of the subsystem that sends them. This
/// header holds what is common to every logger: the minimum level, where messages go, and how they look.
namespace rtype::log
{

/// Importance of a message, from the most detailed to the most serious.
enum class Level {
    /// Very detailed tracing, for instance every packet received.
    Trace,
    /// Information useful when debugging, hidden by default.
    Debug,
    /// Normal events: a client connected, a level started.
    Info,
    /// Something unexpected that the program recovers from.
    Warning,
    /// A failure: an operation could not be done.
    Error,
};

/// One message, as given to a Sink.
struct Record {
    Level level;
    /// Name of the logger that sent the message, for instance "network".
    std::string_view logger;
    std::string_view message;
    std::chrono::system_clock::time_point time;
};

/// Destination of the messages. The default one writes to the console; tests replace it to capture messages.
///
/// Calls to write() are serialised: a sink is never called from two threads at the same time.
class Sink
{
public:
    virtual ~Sink() = default;

    /// Writes one message. The record and its strings are only valid during the call.
    virtual void write(const Record &record) = 0;
};

/// Sets the minimum level of the messages that are written, Info by default. Messages below it are ignored
/// without being formatted, so that disabled Trace messages cost almost nothing.
void setLevel(Level level);
/// Minimum level of the messages that are written.
[[nodiscard]] Level level();

/// Replaces the destination of the messages. nullptr restores the console.
void setSink(std::shared_ptr<Sink> sink);

/// Enables or disables colours on the console. By default, colours are used when the error output is a
/// terminal and the NO_COLOR environment variable is not set.
void setColors(bool enabled);

/// Reads the settings from the environment: RTYPE_LOG_LEVEL sets the level (trace, debug, info, warning or
/// error). Call it once, at the start of the program.
/// @return A description of the problem if RTYPE_LOG_LEVEL is set to an unknown level, nothing otherwise.
std::optional<std::string> configureFromEnvironment();

/// Reads a level from its name, case-insensitive: "trace", "debug", "info", "warning" (or "warn"), "error".
/// @return The level, or nothing if the name is unknown.
[[nodiscard]] std::optional<Level> parseLevel(std::string_view name);

/// Name of a level, in capitals and padded to the same width, for instance "INFO " or "ERROR".
[[nodiscard]] std::string_view levelName(Level level);

/// Formats a message as the console shows it: time, level, logger name and message on one line, with the
/// following lines of the message aligned under the first one.
/// @param colors Whether to add the terminal colour codes.
[[nodiscard]] std::string formatRecord(const Record &record, bool colors);

} // namespace rtype::log
