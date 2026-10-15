#include "rtype/network/Address.hpp"

#include "Asio.hpp"

#include <algorithm>
#include <string>
#include <system_error>

namespace rtype::network
{

namespace
{

// Reads four decimal numbers from 0 to 255 separated by dots, without leading zeros. Written by hand because
// the system parsers differ: Windows accepts "1.2.3.4:80" and ignores the port.
std::optional<std::array<std::uint8_t, 4>> parseIpv4(std::string_view text)
{
    std::array<std::uint8_t, 4> bytes{};
    std::size_t position = 0;
    for (std::size_t part = 0; part < bytes.size(); ++part) {
        if (part > 0) {
            if (position >= text.size() || text[position] != '.') {
                return std::nullopt;
            }
            ++position;
        }
        const std::size_t start = position;
        unsigned value = 0;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9' &&
               position - start < 3) {
            value = value * 10 + static_cast<unsigned>(text[position] - '0');
            ++position;
        }
        const std::size_t digits = position - start;
        if (digits == 0 || value > 255 || (digits > 1 && text[start] == '0')) {
            return std::nullopt;
        }
        bytes[part] = static_cast<std::uint8_t>(value);
    }
    if (position != text.size()) {
        return std::nullopt;
    }
    return bytes;
}

// At least two colons, and only hexadecimal digits, colons, and dots for an IPv4 address at the end
// ("::ffff:1.2.3.4"): refuses what some system parsers accept, such as brackets, a port or a zone ("%eth0")
bool looksLikeIpv6(std::string_view text)
{
    return std::ranges::count(text, ':') >= 2 && std::ranges::all_of(text, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
                      c == ':' || c == '.';
           });
}

} // namespace

std::optional<Address> Address::parse(std::string_view ip, std::uint16_t port)
{
    if (ip.find(':') == std::string_view::npos) {
        const auto bytes = parseIpv4(ip);
        if (!bytes) {
            return std::nullopt;
        }
        return ipv4(*bytes, port);
    }
    if (!looksLikeIpv6(ip)) {
        return std::nullopt;
    }
    std::error_code error;
    const auto address = asio::ip::make_address_v6(std::string(ip), error);
    if (error) {
        return std::nullopt;
    }
    return detail::fromAsio(address, port);
}

Address Address::ipv4(const std::array<std::uint8_t, 4> &bytes, std::uint16_t port)
{
    Address address;
    std::copy(bytes.begin(), bytes.end(), address._bytes.begin());
    address._port = port;
    return address;
}

Address Address::ipv6(const std::array<std::uint8_t, 16> &bytes, std::uint16_t port)
{
    Address address;
    address._bytes = bytes;
    address._ipv6 = true;
    address._port = port;
    return address;
}

Address Address::anyIpv4(std::uint16_t port)
{
    return ipv4({0, 0, 0, 0}, port);
}

Address Address::anyIpv6(std::uint16_t port)
{
    return ipv6({}, port);
}

Address Address::loopbackIpv4(std::uint16_t port)
{
    return ipv4({127, 0, 0, 1}, port);
}

std::vector<std::uint8_t> Address::bytes() const
{
    return {_bytes.begin(), _bytes.begin() + (_ipv6 ? 16 : 4)};
}

Address Address::withPort(std::uint16_t port) const
{
    Address address = *this;
    address._port = port;
    return address;
}

std::string Address::ip() const
{
    return detail::toAsio(*this).to_string();
}

std::string Address::toString() const
{
    if (_ipv6) {
        return std::format("[{}]:{}", ip(), _port);
    }
    return std::format("{}:{}", ip(), _port);
}

std::vector<Address> resolve(std::string_view host, std::uint16_t port)
{
    asio::io_context context;
    asio::ip::tcp::resolver resolver(context);
    std::error_code error;
    const auto results = resolver.resolve(std::string(host), std::to_string(port), error);
    if (error) {
        throw std::system_error(error, std::format("cannot resolve \"{}\"", host));
    }

    std::vector<Address> addresses;
    for (const auto &entry : results) {
        const Address address = detail::fromEndpoint(entry.endpoint());
        if (std::find(addresses.begin(), addresses.end(), address) == addresses.end()) {
            addresses.push_back(address);
        }
    }
    std::stable_partition(addresses.begin(), addresses.end(), [](const Address &a) { return a.isIpv4(); });
    if (addresses.empty()) {
        throw std::system_error(std::make_error_code(std::errc::address_not_available),
                                std::format("cannot resolve \"{}\"", host));
    }
    return addresses;
}

namespace detail
{

const rtype::log::Logger &logger()
{
    static const rtype::log::Logger instance("network");
    return instance;
}

asio::ip::address toAsio(const Address &address)
{
    const std::vector<std::uint8_t> bytes = address.bytes();
    if (address.isIpv6()) {
        asio::ip::address_v6::bytes_type raw{};
        std::copy(bytes.begin(), bytes.end(), raw.begin());
        return asio::ip::address_v6(raw);
    }
    asio::ip::address_v4::bytes_type raw{};
    std::copy(bytes.begin(), bytes.end(), raw.begin());
    return asio::ip::address_v4(raw);
}

Address fromAsio(const asio::ip::address &address, std::uint16_t port)
{
    if (address.is_v6()) {
        const auto raw = address.to_v6().to_bytes();
        std::array<std::uint8_t, 16> bytes{};
        std::copy(raw.begin(), raw.end(), bytes.begin());
        return Address::ipv6(bytes, port);
    }
    const auto raw = address.to_v4().to_bytes();
    return Address::ipv4({raw[0], raw[1], raw[2], raw[3]}, port);
}

} // namespace detail

} // namespace rtype::network

std::size_t
std::hash<rtype::network::Address>::operator()(const rtype::network::Address &address) const noexcept
{
    // FNV-1a over the bytes, the family and the port
    std::size_t hash = 14695981039346656037ULL;
    const auto mix = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    };
    for (const std::uint8_t byte : address.bytes()) {
        mix(byte);
    }
    mix(address.isIpv6() ? 6 : 4);
    mix(static_cast<std::uint8_t>(address.port() >> 8));
    mix(static_cast<std::uint8_t>(address.port() & 0xFF));
    return hash;
}
