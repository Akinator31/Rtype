#include "rtype/network/Address.hpp"

#include "Asio.hpp"

#include <algorithm>
#include <string>
#include <system_error>

namespace rtype::network
{

std::optional<Address> Address::parse(std::string_view ip, std::uint16_t port)
{
    std::error_code error;
    const auto address = asio::ip::make_address(std::string(ip), error);
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
