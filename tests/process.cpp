// Tests of rtype::server::Process: real programs started and stopped, the helper of tests/helpers/ and
// r-type_server itself (docs/architecture/server.md).

#include "Master/Process.hpp"

#include "Common/Options.hpp"
#include "Common/Secret.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <signal.h>

#include <cerrno>
#endif

using namespace rtype::server;
using namespace std::chrono_literals;

namespace
{

/// Long enough for any machine of the CI, short enough for a test that fails.
constexpr std::chrono::milliseconds Patience = 10s;

const std::filesystem::path Child = RTYPE_TEST_CHILD;
const std::filesystem::path Server = RTYPE_TEST_SERVER;

Process child(std::vector<std::string> arguments, ProcessOptions options = {})
{
    return {Child, arguments, std::move(options)};
}

struct TempDirectory {
    std::filesystem::path path;

    explicit TempDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / std::string("rtype_tests_process_").append(name))
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TempDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;
};

/// Waits for a file the child creates once ready.
bool waitForFile(const std::filesystem::path &path)
{
    const auto deadline = std::chrono::steady_clock::now() + Patience;
    while (!std::filesystem::exists(path)) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

std::vector<std::string> linesOf(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    std::vector<std::string> lines;
    for (std::string line; std::getline(file, line);) {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

// --- Starting ---

TEST_CASE("A process gives its exit code once exited", "[process]")
{
    Process process = child({"exit", "7"});

    CHECK(process.pid() > 0);
    CHECK(process.wait(Patience) == 7);
    // Kept once known
    CHECK(process.exitCode() == 7);
    CHECK(process.stop() == 7);
}

TEST_CASE("A process receives its arguments unchanged, without a shell", "[process]")
{
    const TempDirectory directory("arguments");
    const std::filesystem::path file = directory.path / "arguments.txt";
    // What the quoting of the command line of Windows gets wrong when done by hand
    const std::vector<std::string> arguments = {"two words",      "\"quoted\"", "back\\slash\\",
                                                "quoted\\ and\\", "$HOME",      "a;b|c"};
    std::vector<std::string> command = {"arguments", file.string()};
    command.insert(command.end(), arguments.begin(), arguments.end());

    Process process = child(command);

    REQUIRE(process.wait(Patience) == 0);
    CHECK(linesOf(file) == arguments);
}

TEST_CASE("The input is written to the standard input, which is then closed", "[process]")
{
    Process right = child({"input", "the secret"}, {.input = "the secret\n"});
    Process wrong = child({"input", "the secret"}, {.input = "something else\n"});

    CHECK(right.wait(Patience) == 0);
    CHECK(wrong.wait(Patience) == 3);
}

TEST_CASE("Without input, the standard input is closed at once", "[process]")
{
    // Reads its standard input: it would wait forever if it were left open
    Process process = child({"input", "anything"});

    CHECK(process.wait(Patience) == 3);
}

TEST_CASE("An empty argument is refused, since Windows would lose it", "[process]")
{
    const std::vector<std::string> arguments = {"exit", "", "0"};

    CHECK_THROWS_AS(Process(Child, arguments), std::invalid_argument);
}

TEST_CASE("A program that does not exist cannot be started", "[process]")
{
    const std::vector<std::string> none;

    CHECK_THROWS_AS(Process(Child.parent_path() / "no_such_program", none), std::system_error);
}

// --- Stopping ---

TEST_CASE("A process that runs has no exit code, until it is asked to stop", "[process]")
{
    const TempDirectory directory("terminate");
    const std::filesystem::path ready = directory.path / "ready";
    Process process = child({"sleep", ready.string()});
    // On Windows, a program that has only just started does not receive Ctrl+Break yet
    REQUIRE(waitForFile(ready));

    CHECK_FALSE(process.exitCode());
    CHECK_FALSE(process.wait(50ms));

    process.terminate();

    CHECK(process.wait(Patience) == Process::Terminated);
    // Nothing to do once exited
    process.terminate();
    process.kill();
}

TEST_CASE("A process can be killed", "[process]")
{
    Process process = child({"sleep"});

    process.kill();

    CHECK(process.wait(Patience) == Process::Killed);
}

TEST_CASE("stop() asks first, and kills a process still running after the grace", "[process]")
{
    const TempDirectory directory("stubborn");
    const std::filesystem::path ready = directory.path / "ready";
    Process process = child({"stubborn", ready.string()}, {.input = {}, .stopGrace = 200ms});
    REQUIRE(waitForFile(ready));
    const auto start = std::chrono::steady_clock::now();

    CHECK(process.stop() == Process::Killed);

    CHECK(std::chrono::steady_clock::now() - start >= 200ms);
}

TEST_CASE("stop() does not wait for the grace when the process stops when asked", "[process]")
{
    const TempDirectory directory("stop");
    const std::filesystem::path ready = directory.path / "ready";
    Process process = child({"sleep", ready.string()}, {.input = {}, .stopGrace = Patience});
    REQUIRE(waitForFile(ready));
    const auto start = std::chrono::steady_clock::now();

    CHECK(process.stop() == Process::Terminated);

    CHECK(std::chrono::steady_clock::now() - start < Patience);
}

TEST_CASE("A process destroyed while it runs is stopped", "[process]")
{
    const TempDirectory directory("destroyed");
    const std::filesystem::path ready = directory.path / "ready";
    int pid = 0;
    const auto start = std::chrono::steady_clock::now();
    {
        const Process process = child({"stubborn", ready.string()}, {.input = {}, .stopGrace = 100ms});
        pid = process.pid();
        REQUIRE(waitForFile(ready));
    }

    // Killed after the grace, not after the minute the child waits
    CHECK(std::chrono::steady_clock::now() - start < Patience);
#ifndef _WIN32
    // Gone, and reaped: no process has this id any more
    CHECK(::kill(pid, 0) == -1);
    CHECK(errno == ESRCH);
#endif
}

TEST_CASE("A process moved keeps its program, and the one it replaces is stopped", "[process]")
{
    Process first = child({"exit", "5"});
    Process moved = std::move(first);
    CHECK(moved.wait(Patience) == 5);

    moved = child({"sleep"}, {.input = {}, .stopGrace = Patience});
    CHECK_FALSE(moved.exitCode());
    moved.kill();
    CHECK(moved.wait(Patience) == Process::Killed);
}

// --- The server ---

TEST_CASE("The path of the program running is found by the system", "[process]")
{
    const std::filesystem::path path = currentExecutable();

    CHECK(path.is_absolute());
    CHECK(std::filesystem::is_regular_file(path));
    CHECK(path.stem() == "rtype_tests");
}

TEST_CASE("An instance started with its secret runs until asked to stop, then exits properly", "[process]")
{
    Secret secret{};
    secret[0] = std::byte{0x2a};
    const std::vector<std::string> arguments = {"--instance",    "--id", "7",           "--udp-port", "4299",
                                                "--master-port", "4242", "--log-level", "warning"};
    Process instance(Server, arguments, {.input = formatSecret(secret) + "\n", .stopGrace = Patience});

    // Still running: it waits for its game. Its stop signals are set by then
    REQUIRE_FALSE(instance.wait(1s));

    CHECK(instance.stop() == Success);
}

TEST_CASE("An instance started without its secret refuses to run", "[process]")
{
    const std::vector<std::string> arguments = {"--instance",    "--id", "7",           "--udp-port", "4299",
                                                "--master-port", "4242", "--log-level", "error"};
    Process instance(Server, arguments);

    CHECK(instance.wait(Patience) == Failure);
}
