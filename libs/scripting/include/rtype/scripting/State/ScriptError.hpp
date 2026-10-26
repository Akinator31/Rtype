#pragma once

#include <stdexcept>
#include <string>

namespace rtype::scripting
{

/// A script that could not be read, compiled or run. what() starts with the name of its file:
/// "enemies/bydo_scout.lua:3: attempt to index a nil value".
class ScriptError : public std::runtime_error
{
    std::string _file;

public:
    /// Prefixes `message` with `file` unless it already starts with it, as the messages of Lua do.
    ScriptError(std::string file, const std::string &message);

    /// The name the file was run with, e.g. "enemies/bydo_scout.lua".
    [[nodiscard]] const std::string &getFile() const { return _file; }
};

} // namespace rtype::scripting
