#pragma once

#include "Common/Options.hpp"

#include <ostream>

namespace rtype::server
{

/// Runs the master: loads the key of the server, or generates it at the first start (ADR 0021), opens its
/// database and brings its schema up to date (ADR 0023), listens for the clients, and runs until Ctrl+C or
/// SIGTERM. With `check`, stops once the key is loaded, the database open and the port open.
/// @return The exit code of the program.
[[nodiscard]] int runMaster(const MasterOptions &options);

/// Shows the fingerprint of the key of the master on `out`, the one the players trust. Never generates a key.
/// @return The exit code of the program: Failure if the key cannot be read.
[[nodiscard]] int showFingerprint(const MasterOptions &options, std::ostream &out);

} // namespace rtype::server
