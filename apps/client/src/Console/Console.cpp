#include "Console/Console.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <sstream>
#include <utility>

namespace rtype::client
{

namespace
{

std::vector<std::string> splitWords(const std::string &line)
{
    std::istringstream stream(line);
    std::vector<std::string> words;
    for (std::string word; stream >> word;) {
        words.push_back(std::move(word));
    }
    return words;
}

} // namespace

Console::Console()
{
    registerBuiltinCommands();
}

void Console::registerBuiltinCommands()
{
    registerConsoleCommand("help", "help: lists the commands",
                           [](Console &console, const Arguments & /*arguments*/) { console.printHelp(); });
    registerConsoleCommand("clear", "clear: empties the console",
                           [](Console &console, const Arguments & /*arguments*/) { console.clear(); });
}

void Console::printHelp()
{
    for (const auto &[name, command] : _commands) {
        print("  " + command.description);
    }
}

void Console::registerConsoleCommand(const std::string &name, const std::string &description, Command command)
{
    _commands.insert_or_assign(name,
                               RegisteredCommand{.description = description, .run = std::move(command)});
}

bool Console::hasCommand(const std::string &name) const
{
    return _commands.contains(name);
}

void Console::execute(const std::string &line)
{
    std::vector<std::string> words = splitWords(line);
    if (words.empty()) {
        return;
    }
    print("> " + line);
    const std::string name = words.front();
    words.erase(words.begin());
    run(name, words);
}

/// Runs a command, and shows what went wrong in the console rather than letting it stop the game.
void Console::run(const std::string &name, const Arguments &arguments)
{
    const auto command = _commands.find(name);
    if (command == _commands.end()) {
        print("Unknown command: " + name + " (type help)");
        return;
    }
    // Copied: the command may replace itself, or every command, while it runs
    const Command action = command->second.run;
    try {
        action(*this, arguments);
    } catch (const std::exception &error) {
        print("Error: " + std::string(error.what()));
    }
}

void Console::print(const std::string &line)
{
    _lines.push_back(line);
    if (_lines.size() > maxLines) {
        _lines.erase(_lines.begin(), _lines.end() - static_cast<std::ptrdiff_t>(maxLines));
    }
}

void Console::clear()
{
    _lines.clear();
}

std::string Console::getLastLines(std::size_t count) const
{
    const std::size_t first = _lines.size() - std::min(count, _lines.size());
    std::string text;
    for (std::size_t index = first; index < _lines.size(); ++index) {
        if (index != first) {
            text += '\n';
        }
        text += _lines[index];
    }
    return text;
}

} // namespace rtype::client
