#pragma once

#include <system_error>

namespace rtype::network
{

/// Errors of the network library itself, given as `std::error_code` like the errors of the system: a close
/// handler can compare its reason with them, `reason == rtype::network::NetworkError::FrameTooLarge`.
enum class NetworkError {
    /// A TCP frame announces a size of 0: it has no type.
    EmptyFrame = 1,
    /// A TCP frame announces a size above the maximum.
    FrameTooLarge,
};

/// The category of NetworkError, named "rtype.network".
[[nodiscard]] const std::error_category &networkCategory();

[[nodiscard]] std::error_code make_error_code(NetworkError error);

} // namespace rtype::network

/// Lets a NetworkError be compared with and converted to a std::error_code.
template <>
struct std::is_error_code_enum<rtype::network::NetworkError> : std::true_type {};
