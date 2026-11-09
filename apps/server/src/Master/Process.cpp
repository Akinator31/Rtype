#include "Master/Process.hpp"

#include "rtype/log/Logger.hpp"

#include <reproc/reproc.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace rtype::server
{

namespace
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("process");
    return instance;
}

/// reproc returns the error of the system, negated: an errno on Linux, a GetLastError() on Windows.
std::error_code systemError(int result)
{
    return {-result, std::system_category()};
}

/// reproc takes its strings in UTF-8, on Windows too.
std::string toUtf8(const std::filesystem::path &path)
{
    const std::u8string text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}

int toMilliseconds(std::chrono::milliseconds duration)
{
    return static_cast<int>(std::clamp<std::chrono::milliseconds::rep>(duration.count(), 0, INT_MAX));
}

} // namespace

const int Process::Terminated = REPROC_SIGTERM;
const int Process::Killed = REPROC_SIGKILL;

std::filesystem::path currentExecutable()
{
#ifdef _WIN32
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "Cannot find the path of the program");
        }
        // A path cut to the size of the buffer: try again with a larger one
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
#else
    // Linux: the link to the executable, which stays right even if the program was started through a link
    std::error_code error;
    std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) {
        throw std::system_error(error, "Cannot find the path of the program");
    }
    return path;
#endif
}

void Process::Destroy::operator()(reproc_t *process) const
{
    reproc_destroy(process);
}

Process::Process(const std::filesystem::path &program, std::span<const std::string> arguments,
                 Options options)
    : _stopGrace(options.stopGrace)
{
    // reproc only quotes the arguments of the command line of Windows that contain a space or a quote: an
    // empty one would vanish there, and the program would receive the others shifted
    if (std::ranges::any_of(arguments, &std::string::empty)) {
        throw std::invalid_argument("An argument of a process cannot be empty");
    }
    _process.reset(reproc_new());
    if (!_process) {
        throw std::bad_alloc();
    }

    const std::string name = toUtf8(program);
    std::vector<const char *> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(name.c_str());
    for (const std::string &argument : arguments) {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);

    reproc_options settings{};
    settings.redirect.out.type = REPROC_REDIRECT_PARENT;
    settings.redirect.err.type = REPROC_REDIRECT_PARENT;
    // Always given, even empty, so that the standard input is always closed: a program that reads it sees its
    // end instead of waiting forever
    static constexpr std::uint8_t nothing = 0;
    settings.input.data =
        options.input.empty() ? &nothing : reinterpret_cast<const std::uint8_t *>(options.input.data());
    settings.input.size = options.input.size();
    // What reproc_destroy() does if the program still runs: never reached, the destructor stops it first
    settings.stop.first = {REPROC_STOP_TERMINATE, toMilliseconds(_stopGrace)};
    settings.stop.second = {REPROC_STOP_KILL, REPROC_INFINITE};

    const int result = reproc_start(_process.get(), argv.data(), settings);
    if (result < 0) {
        throw std::system_error(systemError(result), "Cannot start " + name);
    }
    _pid = reproc_pid(_process.get());
    logger().debug("Started {} (pid {})", name, _pid);
}

Process::~Process()
{
    if (_process) {
        stop();
    }
}

Process::Process(Process &&other) noexcept
    : _process(std::move(other._process)), _stopGrace(other._stopGrace), _pid(other._pid)
{
}

Process &Process::operator=(Process &&other) noexcept
{
    if (this != &other) {
        if (_process) {
            stop();
        }
        _process = std::move(other._process);
        _stopGrace = other._stopGrace;
        _pid = other._pid;
    }
    return *this;
}

int Process::pid() const
{
    return _pid;
}

std::optional<int> Process::exitCode()
{
    return wait(std::chrono::milliseconds::zero());
}

std::optional<int> Process::wait(std::chrono::milliseconds timeout)
{
    const int result = reproc_wait(_process.get(), toMilliseconds(timeout));
    if (result == REPROC_ETIMEDOUT) {
        return std::nullopt;
    }
    if (result < 0) {
        throw std::system_error(systemError(result), "Cannot wait for process " + std::to_string(_pid));
    }
    return result;
}

void Process::terminate()
{
    if (exitCode()) {
        return;
    }
    const int result = reproc_terminate(_process.get());
    if (result < 0) {
        throw std::system_error(systemError(result), "Cannot stop process " + std::to_string(_pid));
    }
}

void Process::kill()
{
    if (exitCode()) {
        return;
    }
    const int result = reproc_kill(_process.get());
    if (result < 0) {
        throw std::system_error(systemError(result), "Cannot kill process " + std::to_string(_pid));
    }
}

int Process::stop() noexcept
{
    reproc_t *process = _process.get();
    int result = reproc_wait(process, 0);
    if (result >= 0) {
        return result;
    }

    // Asked first: an instance tells its players before exiting
    result = reproc_terminate(process);
    if (result < 0) {
        // On Windows, when this program has no console to send Ctrl+Break through
        logger().warning("Cannot ask process {} to stop ({}): killing it", _pid,
                         systemError(result).message());
    } else {
        // The whole grace: on Windows, a wait of n ms follows the tick of the system and may end a few ms
        // early
        const auto deadline = std::chrono::steady_clock::now() + _stopGrace;
        do {
            const auto left =
                std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            result = reproc_wait(process, toMilliseconds(std::max(left, std::chrono::milliseconds(1))));
        } while (result == REPROC_ETIMEDOUT && std::chrono::steady_clock::now() < deadline);
        if (result >= 0) {
            return result;
        }
        logger().warning("Process {} still runs {} ms after being asked to stop: killing it", _pid,
                         _stopGrace.count());
    }

    result = reproc_kill(process);
    if (result < 0) {
        logger().error("Cannot kill process {}: {}", _pid, systemError(result).message());
        return Killed;
    }
    result = reproc_wait(process, REPROC_INFINITE);
    return result >= 0 ? result : Killed;
}

} // namespace rtype::server
