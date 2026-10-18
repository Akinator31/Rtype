#pragma once

#include "Common/Options.hpp"

namespace rtype::server
{

/// Runs the master: loads the key of the server, listens for the clients, and runs until Ctrl+C or SIGTERM.
/// With `check`, stops once the key is loaded and the port open.
/// @return The exit code of the program.
[[nodiscard]] int runMaster(const MasterOptions &options);

} // namespace rtype::server
