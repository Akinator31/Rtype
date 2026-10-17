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
    /// The TLS server presented a key other than the pinned one: it is not our server.
    UntrustedServer,
    /// The TCP connection ended before the TLS handshake was done.
    HandshakeInterrupted,
};

/// The category of NetworkError, named "rtype.network".
[[nodiscard]] const std::error_category &networkCategory();

/// The category of the errors reported by Mbed TLS, named "rtype.tls": a corrupted record, a peer that does
/// not speak TLS 1.3, an alert sent by the peer... Their message is the one of Mbed TLS, for the logs:
/// `reason.category() == rtype::network::tlsCategory()` tells that the TLS layer ended the connection.
[[nodiscard]] const std::error_category &tlsCategory();

[[nodiscard]] std::error_code make_error_code(NetworkError error);

} // namespace rtype::network

/// Lets a NetworkError be compared with and converted to a std::error_code.
template <>
struct std::is_error_code_enum<rtype::network::NetworkError> : std::true_type {};
