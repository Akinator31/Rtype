#include "Web/Visitor.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace rtype::server
{

namespace
{

/// The address without its port, and an IPv4 address written in IPv6 as IPv4: a listener on `[::]` sees its
/// IPv4 clients as `::ffff:1.2.3.4`.
rtype::network::Address plain(const rtype::network::Address &address)
{
    const std::vector<std::uint8_t> bytes = address.bytes();
    if (address.isIpv6() &&
        std::all_of(bytes.begin(), bytes.begin() + 10, [](std::uint8_t b) { return b == 0; }) &&
        bytes[10] == 0xFF && bytes[11] == 0xFF) {
        return rtype::network::Address::ipv4({bytes[12], bytes[13], bytes[14], bytes[15]}, 0);
    }
    return address.withPort(0);
}

bool trusted(const rtype::network::Address &address, std::span<const Subnet> proxies)
{
    return std::ranges::any_of(proxies, [&](const Subnet &subnet) { return inSubnet(address, subnet); });
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

/// One entry of X-Forwarded-For: an address alone, as nginx, Caddy and Traefik write it.
std::optional<rtype::network::Address> parseEntry(std::string_view entry)
{
    const auto address = rtype::network::Address::parse(trim(entry), 0);
    if (!address) {
        return std::nullopt;
    }
    return plain(*address);
}

} // namespace

bool inSubnet(const rtype::network::Address &address, const Subnet &subnet)
{
    const rtype::network::Address candidate = plain(address);
    if (candidate.isIpv4() != subnet.address.isIpv4()) {
        return false;
    }
    const std::vector<std::uint8_t> bytes = candidate.bytes();
    const std::vector<std::uint8_t> network = subnet.address.bytes();
    for (unsigned bit = 0; bit < subnet.prefix; ++bit) {
        const unsigned mask = 0x80U >> (bit % 8);
        if ((bytes[bit / 8] & mask) != (network[bit / 8] & mask)) {
            return false;
        }
    }
    return true;
}

rtype::network::Address visitorAddress(const rtype::network::Address &peer, std::string_view forwardedFor,
                                       std::span<const Subnet> trustedProxies)
{
    rtype::network::Address visitor = plain(peer);
    while (trusted(visitor, trustedProxies) && !trim(forwardedFor).empty()) {
        const std::size_t comma = forwardedFor.rfind(',');
        const std::string_view entry =
            comma == std::string_view::npos ? forwardedFor : forwardedFor.substr(comma + 1);
        forwardedFor = comma == std::string_view::npos ? std::string_view{} : forwardedFor.substr(0, comma);
        const auto address = parseEntry(entry);
        if (!address) {
            break;
        }
        visitor = *address;
    }
    return visitor;
}

} // namespace rtype::server
