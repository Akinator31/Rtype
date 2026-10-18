// A program for the tests of rtype::server::Process (tests/process.cpp): what it does depends on its first
// argument.
//
//   exit <code>                    exits with this code
//   input <line>                   exits with 0 if the standard input is this line then its end, 3 otherwise
//   arguments <file> <args>...     writes its arguments to the file, one per line, and exits with 0
//   sleep [file] [anything]...     creates the file if given, to say it is ready, and waits until stopped
//   stubborn <file>                ignores the requests to stop, creates the file to say so, and waits
//
// It never waits more than a minute: a test that fails leaves no program behind for long.

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace
{

constexpr int Usage = 2;
constexpr int Mismatch = 3;
constexpr int TooLong = 4;

int sleepAWhile()
{
    std::this_thread::sleep_for(std::chrono::minutes(1));
    return TooLong;
}

bool inputIs(std::string_view expected)
{
    std::string line;
    if (!std::getline(std::cin, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    // Then the end of the stream: the input was closed
    return line == expected && std::cin.peek() == std::char_traits<char>::eof();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        return Usage;
    }
    const std::string_view mode = argv[1];

    if (mode == "exit" && argc == 3) {
        return std::atoi(argv[2]);
    }
    if (mode == "input" && argc == 3) {
        return inputIs(argv[2]) ? 0 : Mismatch;
    }
    if (mode == "arguments" && argc >= 3) {
        std::ofstream file(argv[2], std::ios::binary);
        for (int i = 3; i < argc; ++i) {
            file << argv[i] << '\n';
        }
        return file ? 0 : Mismatch;
    }
    // Arguments after the file are ignored: it can stand for a program given more
    if (mode == "sleep") {
        if (argc >= 3 && argv[2][0] != '-') {
            std::ofstream(argv[2]) << "ready\n";
        }
        return sleepAWhile();
    }
    if (mode == "stubborn" && argc == 3) {
        std::signal(SIGTERM, SIG_IGN);
#ifdef SIGBREAK
        std::signal(SIGBREAK, SIG_IGN);
#endif
        std::ofstream(argv[2]) << "ready\n";
        return sleepAWhile();
    }
    return Usage;
}
