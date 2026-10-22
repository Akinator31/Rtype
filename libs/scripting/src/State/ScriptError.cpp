#include "rtype/scripting/State/ScriptError.hpp"

#include <format>
#include <utility>

namespace rtype::scripting
{

namespace
{

std::string nameMessage(const std::string &file, const std::string &message)
{
    if (message.starts_with(file)) {
        return message;
    }
    return std::format("{}: {}", file, message);
}

} // namespace

ScriptError::ScriptError(std::string file, const std::string &message)
    : std::runtime_error(nameMessage(file, message)), _file(std::move(file))
{
}

} // namespace rtype::scripting
