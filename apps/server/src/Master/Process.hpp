#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

struct reproc_t;

namespace rtype::server
{

/// The path of the program running, as the system knows it: the master starts its instances with its own
/// executable (ADR 0013). Not argv[0], which can be relative, a name found in the PATH, or anything.
/// @throws std::system_error if the system cannot tell.
[[nodiscard]] std::filesystem::path currentExecutable();

/// How a Process starts and stops (outside the class, so that it can be a default argument).
struct ProcessOptions {
    /// Written to the standard input before the program starts, which is then closed. At most a few
    /// kilobytes: the size of a pipe.
    std::string input;
    /// How long stop() waits after asking the program to stop, before killing it.
    std::chrono::milliseconds stopGrace{5000};
};

/// A program started by this one, on Linux and Windows: the master starts one per game (ADR 0013). It hides
/// reproc, the library that starts and stops it.
///
/// @code
/// rtype::server::Process instance(rtype::server::currentExecutable(),
///                                 std::vector<std::string>{"--instance", "--id", "7", ...},
///                                 {.input = formatSecret(secret) + "\n"});
///
/// if (const auto code = instance.exitCode()) { ... } // it has exited: never blocks
/// instance.stop();                                     // asks it to stop, kills it if it does not
/// @endcode
///
/// - Its standard input receives `ProcessOptions::input`, then is closed: the program reads it, then the
///   end of the stream. Nothing else can be written later, so a program that died early cannot break this
///   one.
/// - Its standard output and error are those of this program: the logs of the instances go with those of the
///   master.
/// - Destroying a Process stops the program if it still runs, like stop(): the master leaves no program
///   behind, even on an exception.
///
/// Not thread-safe: use it from one thread, the event loop of the master.
class Process
{
public:
    using Options = ProcessOptions;

    /// The exit code of a program stopped by terminate(): 128 + SIGTERM, as a shell shows it. On Windows,
    /// the code of a program ended by Ctrl+Break gives the same value.
    static const int Terminated;
    /// The exit code of a program stopped by kill(): 128 + SIGKILL, on Windows too.
    static const int Killed;

    /// Starts a program with its arguments, which it receives unchanged (spaces, quotes...): there is no
    /// shell.
    /// @throws std::system_error if it cannot start: not found, not executable.
    Process(const std::filesystem::path &program, std::span<const std::string> arguments,
            Options options = {});
    ~Process();

    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;
    Process(Process &&other) noexcept;
    /// Stops the program it had, like the destructor.
    Process &operator=(Process &&other) noexcept;

    /// Its process id, for the logs.
    [[nodiscard]] int pid() const;

    /// Tells whether it has exited, without waiting.
    /// @return Its exit code once exited, nothing while it runs.
    /// @throws std::system_error if the system cannot tell.
    [[nodiscard]] std::optional<int> exitCode();

    /// Waits for it to exit, at most `timeout`. Blocks: for the tests and the end of the master.
    /// @return Its exit code, or nothing if it still runs.
    /// @throws std::system_error if the system cannot tell.
    std::optional<int> wait(std::chrono::milliseconds timeout);

    /// Asks it to stop: SIGTERM on Linux, Ctrl+Break on Windows. A server ends its work first (ADR 0013), so
    /// wait for it with exitCode() or wait(). Does nothing once it has exited.
    /// @throws std::system_error if the request cannot be sent.
    void terminate();

    /// Stops it at once: SIGKILL on Linux, TerminateProcess on Windows. Does nothing once it has exited.
    /// @throws std::system_error if it cannot be killed.
    void kill();

    /// Asks it to stop, waits `ProcessOptions::stopGrace`, and kills it if it still runs. Blocks until it has
    /// exited.
    /// @return Its exit code.
    int stop() noexcept;

private:
    struct Destroy {
        void operator()(reproc_t *process) const;
    };

    std::unique_ptr<reproc_t, Destroy> _process;
    std::chrono::milliseconds _stopGrace;
    int _pid = 0;
};

} // namespace rtype::server
