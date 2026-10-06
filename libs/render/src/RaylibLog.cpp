#include "RaylibLog.hpp"

#include "rtype/log/Logger.hpp"

#include <raylib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace rtype::render::detail
{

namespace
{

// raylib's INFO messages describe every texture loaded and every setting of the window: they are details
// for us, shown at the Debug level.
//
// raylib stops the program after a FATAL message, except when its messages are redirected, as here. The only
// fatal message on desktop is a failure to open the window, which Window detects and reports with an
// exception: the error then goes through the normal error handling of the program.
log::Level toLevel(int raylibLevel)
{
    switch (raylibLevel) {
    case LOG_TRACE:
        return log::Level::Trace;
    case LOG_DEBUG:
    case LOG_INFO:
        return log::Level::Debug;
    case LOG_WARNING:
        return log::Level::Warning;
    default:
        return log::Level::Error;
    }
}

void forward(int raylibLevel, const char *format, va_list arguments)
{
    static const log::Logger logger("raylib");

    const log::Level level = toLevel(raylibLevel);
    if (!logger.isEnabled(level)) {
        return;
    }
    // raylib gives a printf format: measure the text first, then write it
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        return;
    }
    std::string text(static_cast<std::size_t>(length), '\0');
    std::vsnprintf(text.data(), text.size() + 1, format, arguments);
    logger.log(level, "{}", text);
}

} // namespace

void redirectRaylibLogs()
{
    // Every message is forwarded: the level of our logger decides which ones are shown
    SetTraceLogLevel(LOG_ALL);
    SetTraceLogCallback(forward);
}

} // namespace rtype::render::detail
