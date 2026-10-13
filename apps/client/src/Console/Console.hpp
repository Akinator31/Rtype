#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace rtype::client
{

/// The commands typed in the console, and the lines it shows.
///
/// A command is a name and a function: the line typed is split on spaces, the first word picks the command,
/// the other words are its arguments. "help" and "clear" are always there.
/// @code
/// console.registerConsoleCommand("say", "say <text...>: writes the text in the console",
///                                [](Console &console, const Console::Arguments &arguments) {
///                                    for (const std::string &word : arguments) {
///                                        console.print(word);
///                                    }
///                                });
/// console.execute("say hello world");
/// @endcode
class Console
{
public:
    /// The words typed after the name of the command.
    using Arguments = std::vector<std::string>;
    /// What a command does. It writes its result with Console::print(); an exception it throws is shown in
    /// the console instead of stopping the game.
    using Command = std::function<void(Console &console, const Arguments &arguments)>;

    /// Number of lines kept: the oldest ones are forgotten.
    static constexpr std::size_t maxLines = 200;

private:
    struct RegisteredCommand {
        std::string description;
        Command run;
    };

    // Sorted by name, so that help lists them in alphabetical order
    std::map<std::string, RegisteredCommand> _commands;
    std::vector<std::string> _lines;

    void registerBuiltinCommands();
    void printHelp();
    void run(const std::string &name, const Arguments &arguments);

public:
    /// Registers "help" and "clear".
    Console();

    /// Adds a command, or replaces the one with the same name.
    /// @param name What to type to run it: a single word.
    /// @param description Shown by "help", usually with its arguments: "scene <name>: opens a scene".
    void registerConsoleCommand(const std::string &name, const std::string &description, Command command);

    [[nodiscard]] bool hasCommand(const std::string &name) const;

    /// Writes the line, then runs the command it names. Does nothing if the line is blank.
    void execute(const std::string &line);

    /// Adds a line at the bottom of the console.
    void print(const std::string &line);
    /// Removes every line.
    void clear();

    /// The lines, from the oldest to the newest.
    [[nodiscard]] const std::vector<std::string> &getLines() const { return _lines; }
    /// The newest lines, at most `count`, joined with line feeds to be drawn as one text.
    [[nodiscard]] std::string getLastLines(std::size_t count) const;
};

} // namespace rtype::client
