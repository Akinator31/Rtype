#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// Networking shared by the client and the server: addresses, UDP and TCP sockets, and the event loop that
/// runs them. Built on Asio, which never appears in these headers (ADR 0011).
namespace rtype::network
{

/// An IP address (IPv4 or IPv6) and a port: identifies one program on one machine, for instance
/// `127.0.0.1:4242`.
class Address
{
private:
    /// IPv4 addresses use the first 4 bytes.
    std::array<std::uint8_t, 16> _bytes{};
    bool _ipv6 = false;
    std::uint16_t _port = 0;

public:
    /// `0.0.0.0:0`: any IPv4 interface, port chosen by the system.
    Address() = default;

    /// Reads an IP address written as text, `"192.168.1.10"` or `"::1"`. Host names are not accepted: use
    /// resolve() for them.
    /// @return The address, or nothing if the text is not an IP address.
    [[nodiscard]] static std::optional<Address> parse(std::string_view ip, std::uint16_t port);

    /// An IPv4 address from its four bytes, for instance `ipv4({127, 0, 0, 1}, 4242)`.
    [[nodiscard]] static Address ipv4(const std::array<std::uint8_t, 4> &bytes, std::uint16_t port);
    /// An IPv6 address from its sixteen bytes.
    [[nodiscard]] static Address ipv6(const std::array<std::uint8_t, 16> &bytes, std::uint16_t port);

    /// `0.0.0.0`: every IPv4 interface of the machine. To bind a socket that accepts traffic from anywhere.
    [[nodiscard]] static Address anyIpv4(std::uint16_t port = 0);
    /// `::`: every IPv6 interface of the machine.
    [[nodiscard]] static Address anyIpv6(std::uint16_t port = 0);
    /// `127.0.0.1`: the machine itself. Traffic to it never leaves the machine.
    [[nodiscard]] static Address loopbackIpv4(std::uint16_t port = 0);

    [[nodiscard]] bool isIpv4() const { return !_ipv6; }
    [[nodiscard]] bool isIpv6() const { return _ipv6; }
    [[nodiscard]] std::uint16_t port() const { return _port; }

    /// The bytes of the IP address: 4 for IPv4, 16 for IPv6.
    [[nodiscard]] std::vector<std::uint8_t> bytes() const;

    /// The same IP address with another port.
    [[nodiscard]] Address withPort(std::uint16_t port) const;

    /// The IP address alone, `"127.0.0.1"` or `"::1"`.
    [[nodiscard]] std::string ip() const;
    /// The IP address and the port, `"127.0.0.1:4242"` or `"[::1]:4242"`.
    [[nodiscard]] std::string toString() const;

    friend bool operator==(const Address &, const Address &) = default;
};

/// Finds the addresses of a host name (`"localhost"`, `"rtype.example.com"`) or an IP address written as
/// text. IPv4 addresses come first.
///
/// Blocks while the name is looked up, which may take seconds if the network is slow: call it at startup or
/// on a thread that can wait, never in the game loop.
/// @throws std::system_error if the name cannot be resolved.
[[nodiscard]] std::vector<Address> resolve(std::string_view host, std::uint16_t port);

} // namespace rtype::network

/// Hash of an address, to use it as a key of an `std::unordered_map`.
template <>
struct std::hash<rtype::network::Address> {
    std::size_t operator()(const rtype::network::Address &address) const noexcept;
};

/// Formats an address as Address::toString() does, so that it can be logged directly:
/// `logger.info("Client connected from {}", address);`
template <>
struct std::formatter<rtype::network::Address> : std::formatter<std::string> {
    auto format(const rtype::network::Address &address, std::format_context &context) const
    {
        return std::formatter<std::string>::format(address.toString(), context);
    }
};
