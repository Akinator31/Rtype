#include "rtype/log/Log.hpp"

#include "Output.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace rtype::log
{

namespace
{

// Terminal colour codes (ANSI escape sequences)
constexpr std::string_view reset = "\x1b[0m";
constexpr std::string_view dim = "\x1b[2m";
constexpr std::string_view grey = "\x1b[90m";
constexpr std::string_view cyan = "\x1b[36m";
constexpr std::string_view green = "\x1b[32m";
constexpr std::string_view boldYellow = "\x1b[1;33m";
constexpr std::string_view yellow = "\x1b[33m";
constexpr std::string_view boldRed = "\x1b[1;31m";
constexpr std::string_view red = "\x1b[31m";

// Colours given to logger names. Red and grey are left out: they already mean "error" and "detail".
constexpr std::string_view nameColors[] = {
    "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[32m", "\x1b[94m", "\x1b[95m", "\x1b[96m", "\x1b[92m",
};

// Width of the logger name column: longer names push the message to the right
constexpr std::size_t nameWidth = 8;

std::string_view levelColor(Level level)
{
    switch (level) {
    case Level::Trace:
        return grey;
    case Level::Debug:
        return cyan;
    case Level::Info:
        return green;
    case Level::Warning:
        return boldYellow;
    case Level::Error:
        return boldRed;
    }
    return reset;
}

// Warnings and errors are tinted so that they stand out in a long log; other messages keep the default colour
std::string_view messageColor(Level level)
{
    switch (level) {
    case Level::Warning:
        return yellow;
    case Level::Error:
        return red;
    default:
        return {};
    }
}

// Each logger keeps the same colour from one run to the next: it is chosen from a hash of its name (FNV-1a,
// which gives the same result on every platform, unlike std::hash)
std::string_view nameColor(std::string_view name)
{
    std::uint32_t hash = 2166136261u;
    for (const char character : name) {
        hash = (hash ^ static_cast<unsigned char>(character)) * 16777619u;
    }
    return nameColors[hash % std::size(nameColors)];
}

std::optional<std::string> environmentVariable(const char *name)
{
#ifdef _WIN32
    // getenv() is deprecated by MSVC
    char *value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char *value = std::getenv(name);
    return value == nullptr ? std::nullopt : std::optional<std::string>(value);
#endif
}

std::tm localTime(std::time_t time)
{
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &time);
#else
    localtime_r(&time, &result);
#endif
    return result;
}

// Colours are used when the error output is a terminal, unless NO_COLOR is set (https://no-color.org).
// The Windows console only understands colour codes once asked to.
bool detectColors()
{
    if (environmentVariable("NO_COLOR").has_value()) {
        return false;
    }
#ifdef _WIN32
    if (_isatty(_fileno(stderr)) == 0) {
        return false;
    }
    HANDLE console = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    return console != INVALID_HANDLE_VALUE && GetConsoleMode(console, &mode) != 0 &&
           SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

// -1 until setColors() is called: the detected value is used meanwhile
std::atomic<int> colorsSetting{-1};

bool useColors()
{
    static const bool detected = detectColors();
    const int setting = colorsSetting.load();
    return setting < 0 ? detected : setting != 0;
}

// Writes each message on its own line of the error output, which is not buffered: a message is visible
// even if the program crashes right after it
class ConsoleSink : public Sink
{
public:
    void write(const Record &record) override
    {
        std::string line = formatRecord(record, useColors());
        line += '\n';
        std::fwrite(line.data(), 1, line.size(), stderr);
    }
};

struct State {
    std::atomic<Level> level{Level::Info};
    std::mutex mutex;
    std::shared_ptr<Sink> sink;
    ConsoleSink console;
};

// Never destroyed: objects destroyed at the end of the program may still log from their destructor
State &state()
{
    static State *const instance = new State;
    return *instance;
}

} // namespace

void setLevel(Level newLevel)
{
    state().level.store(newLevel);
}

Level level()
{
    return state().level.load();
}

void setSink(std::shared_ptr<Sink> sink)
{
    State &current = state();
    const std::scoped_lock lock(current.mutex);
    current.sink = std::move(sink);
}

void setColors(bool enabled)
{
    colorsSetting.store(enabled ? 1 : 0);
}

std::optional<std::string> configureFromEnvironment()
{
    const std::optional<std::string> name = environmentVariable("RTYPE_LOG_LEVEL");
    if (!name.has_value() || name->empty()) {
        return std::nullopt;
    }
    const std::optional<Level> parsed = parseLevel(*name);
    if (!parsed.has_value()) {
        return std::format(
            "Unknown log level \"{}\" in RTYPE_LOG_LEVEL, expected trace, debug, info, warning or "
            "error",
            *name);
    }
    setLevel(*parsed);
    return std::nullopt;
}

std::optional<Level> parseLevel(std::string_view name)
{
    std::string lower(name);
    std::ranges::transform(lower, lower.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    if (lower == "trace") {
        return Level::Trace;
    }
    if (lower == "debug") {
        return Level::Debug;
    }
    if (lower == "info") {
        return Level::Info;
    }
    if (lower == "warning" || lower == "warn") {
        return Level::Warning;
    }
    if (lower == "error") {
        return Level::Error;
    }
    return std::nullopt;
}

std::string_view levelName(Level level)
{
    switch (level) {
    case Level::Trace:
        return "TRACE";
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO ";
    case Level::Warning:
        return "WARN ";
    case Level::Error:
        return "ERROR";
    }
    return "?????";
}

std::string formatRecord(const Record &record, bool colors)
{
    using namespace std::chrono;

    const std::tm time = localTime(system_clock::to_time_t(record.time));
    const auto millis = duration_cast<milliseconds>(record.time.time_since_epoch()) % 1000;
    const std::string timestamp =
        std::format("{:02}:{:02}:{:02}.{:03}", time.tm_hour, time.tm_min, time.tm_sec, millis.count());
    const std::string name = std::format("{:<{}}", record.logger, nameWidth);

    // Lines after the first one start under the message: same width as what comes before it
    const std::string indent(timestamp.size() + 1 + levelName(record.level).size() + 1 + name.size() + 1,
                             ' ');
    std::string message;
    for (const char character : record.message) {
        message += character;
        if (character == '\n') {
            message += indent;
        }
    }

    if (!colors) {
        return std::format("{} {} {} {}", timestamp, levelName(record.level), name, message);
    }
    const std::string_view tint = messageColor(record.level);
    return std::format("{}{}{} {}{}{} {}{}{} {}{}{}", dim, timestamp, reset, levelColor(record.level),
                       levelName(record.level), reset, nameColor(record.logger), name, reset, tint, message,
                       tint.empty() ? std::string_view{} : reset);
}

namespace detail
{

void output(const Record &record)
{
    State &current = state();
    const std::scoped_lock lock(current.mutex);
    Sink &sink = current.sink ? *current.sink : current.console;
    sink.write(record);
}

} // namespace detail

} // namespace rtype::log
