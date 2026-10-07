#pragma once

#include "rtype/log/Log.hpp"

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace rtype::log
{

/// Writes messages on behalf of one subsystem, whose name appears in front of each message.
///
/// Messages use the std::format syntax, checked at compile time:
/// @code
/// const rtype::log::Logger logger("network");
///
/// logger.info("Client {} connected from {}:{}", id, address, port);
/// logger.warning("Packet of {} bytes is too short, ignored", size);
/// @endcode
///
/// A logger only holds its name: create one per subsystem and keep it, or create it where it is needed.
/// Loggers can be used from several threads at the same time.
class Logger
{
private:
    std::string _name;

    void write(Level level, std::string_view message) const;

public:
    /// @param name Name of the subsystem, short and lower case, for instance "network" or "render".
    explicit Logger(std::string name);

    /// Name given to the logger.
    [[nodiscard]] const std::string &name() const { return _name; }

    /// Tells whether messages of this level are written, for instance to skip computing an expensive value
    /// that would only be logged.
    [[nodiscard]] bool isEnabled(Level messageLevel) const { return messageLevel >= level(); }

    /// Writes a message of the given level, if it is enabled.
    template <typename... Args>
    void log(Level messageLevel, std::format_string<Args...> format, Args &&...args) const
    {
        if (isEnabled(messageLevel)) {
            write(messageLevel, std::format(format, std::forward<Args>(args)...));
        }
    }

    /// Writes a Trace message: very detailed tracing.
    template <typename... Args>
    void trace(std::format_string<Args...> format, Args &&...args) const
    {
        log(Level::Trace, format, std::forward<Args>(args)...);
    }

    /// Writes a Debug message: information useful when debugging.
    template <typename... Args>
    void debug(std::format_string<Args...> format, Args &&...args) const
    {
        log(Level::Debug, format, std::forward<Args>(args)...);
    }

    /// Writes an Info message: a normal event.
    template <typename... Args>
    void info(std::format_string<Args...> format, Args &&...args) const
    {
        log(Level::Info, format, std::forward<Args>(args)...);
    }

    /// Writes a Warning message: something unexpected that the program recovers from.
    template <typename... Args>
    void warning(std::format_string<Args...> format, Args &&...args) const
    {
        log(Level::Warning, format, std::forward<Args>(args)...);
    }

    /// Writes an Error message: an operation failed.
    template <typename... Args>
    void error(std::format_string<Args...> format, Args &&...args) const
    {
        log(Level::Error, format, std::forward<Args>(args)...);
    }
};

} // namespace rtype::log
