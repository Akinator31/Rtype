#pragma once

#include "Common/Options.hpp"

#include <istream>

namespace rtype::server
{

/// Runs an instance: reads its secret on `input` (the standard input, written by the master), then runs until
/// Ctrl+C or SIGTERM. In standalone mode, opens its UDP port at once, without a secret nor a master.
/// @return The exit code of the program.
[[nodiscard]] int runInstance(const InstanceOptions &options, std::istream &input);

} // namespace rtype::server
