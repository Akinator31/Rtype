#pragma once

#include "Common/Settings.hpp"
#include "rtype/network/Address.hpp"

#include <span>
#include <string_view>

namespace rtype::server
{

/// Tells whether `address` is in `subnet`. An IPv4 address written in IPv6 (`::ffff:1.2.3.4`) is the IPv4
/// address it holds.
[[nodiscard]] bool inSubnet(const rtype::network::Address &address, const Subnet &subnet);

/// The address of the visitor of the panel (docs/architecture/web.md#the-proxy). The peer is the one who
/// connected: when it is a trusted proxy, its `X-Forwarded-For` is read from right to left, each proxy
/// appending the address it was reached from, and the first address that is not a trusted proxy is the
/// visitor. The entries on its left were written by the visitor, who can write anything: they are never
/// read. An entry that is not an address stops the reading at the proxy that wrote it.
/// @param forwardedFor The `X-Forwarded-For` header, empty if the request has none.
/// @return The address, its port 0.
[[nodiscard]] rtype::network::Address visitorAddress(const rtype::network::Address &peer,
                                                     std::string_view forwardedFor,
                                                     std::span<const Subnet> trustedProxies);

} // namespace rtype::server
