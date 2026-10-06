#include "rtype/log/Logger.hpp"

#include "Output.hpp"

#include <chrono>

namespace rtype::log
{

Logger::Logger(std::string name) : _name(std::move(name)) {}

void Logger::write(Level level, std::string_view message) const
{
    detail::output({level, _name, message, std::chrono::system_clock::now()});
}

} // namespace rtype::log
