#pragma once

#include "Common/Options.hpp"

#include <istream>

namespace rtype::server
{

/// Runs an instance, one game (docs/protocol/instances.md): reads its secret on `input` (the standard input,
/// written by the master), connects to the master, opens its UDP port once configured, and runs the game
/// until SHUTDOWN, the loss of the master, Ctrl+C or SIGTERM. In standalone mode, opens its UDP port at once,
/// without a secret nor a master, with the sessions of rtype/game/Standalone.hpp.
/// @return The exit code of the program.
[[nodiscard]] int runInstance(const InstanceOptions &options, std::istream &input);

} // namespace rtype::server
