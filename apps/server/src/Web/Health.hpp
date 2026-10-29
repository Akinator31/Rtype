#pragma once

#include "Common/Options.hpp"
#include "rtype/network/Address.hpp"

#include <chrono>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace rtype::server
{

/// How long --health waits for the master, from the connection to the last byte of its answer.
inline constexpr std::chrono::seconds HealthTimeout{5};

/// Reads the HTTP response of `/api/health`, whole.
/// @return Nothing when the master is healthy, `200 {"status":"ok"}`; else what is wrong, for the user.
[[nodiscard]] std::optional<std::string> healthProblem(std::string_view response);

/// Asks `/api/health` of the web server that listens on `panel`, and waits for its answer, at most `timeout`.
/// The answer comes from the event loop of the master: a master whose loop is stuck is not healthy.
/// @return Nothing when the master is healthy; else what is wrong, for the user.
[[nodiscard]] std::optional<std::string> askHealth(const rtype::network::Address &panel,
                                                   std::chrono::milliseconds timeout = HealthTimeout);

/// --health: asks the master that runs on this machine with these options, at the address its settings give
/// the panel, whether it is healthy, and shows `healthy` on `out` when it is. The health check of the Docker
/// image, which has no curl (docs/guides/deploy-with-docker.md).
/// @return The exit code of the program: Failure when the master is not healthy, or cannot be asked.
[[nodiscard]] int showHealth(const MasterOptions &options, std::ostream &out);

} // namespace rtype::server
