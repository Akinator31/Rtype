#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace rtype::client
{

/// A server the team hosts, built into the client (ADR 0020): the Online menu offers it in one click, and the
/// client trusts its keys without asking the player.
struct OfficialServer {
    /// Shown on its button.
    std::string_view name;
    /// Its domain name, or its IP address.
    std::string_view host;
    std::uint16_t port = 4242;
    /// The fingerprints of its keys (`r-type_keygen --fingerprint`): the key the server uses, then its backup
    /// key, kept offline, which the server switches to if the first one leaks or is lost.
    std::array<std::string_view, 2> keys;
};

/// The official servers. Their keys are public: changing them is a commit, and a new release of the client.
///
/// To add one, give its size to the array and its entry:
/// @code
/// inline constexpr std::array<OfficialServer, 1> OfficialServers{{
///     {.name = "Official server", .host = "rtype.example.org", .port = 4242,
///      .keys = {"<fingerprint of the main key>", "<fingerprint of the backup key>"}},
/// }};
/// @endcode
inline constexpr std::array<OfficialServer, 0> OfficialServers{};

} // namespace rtype::client
