#pragma once

#include <istream>
#include <ostream>
#include <span>
#include <string_view>

namespace rtype::server
{

/// The version of the server, the one of the project.
[[nodiscard]] std::string_view version();

/// Runs the program: reads the arguments, then runs the master or an instance. What the program shows
/// (--help, --version, --fingerprint) goes to `out`, the errors to the logs; an instance reads its secret on
/// `in`.
/// @return The exit code of the program.
[[nodiscard]] int run(std::span<const std::string_view> arguments, std::ostream &out, std::istream &in);

} // namespace rtype::server
