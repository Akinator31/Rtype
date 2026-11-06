#include "Common/RelayFrame.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace rtype::server
{

namespace
{

constexpr std::uint8_t FamilyIpv4 = 4;
constexpr std::uint8_t FamilyIpv6 = 6;
constexpr std::size_t AddressOffset = 1;
constexpr std::size_t AddressSize = 16;
constexpr std::size_t PortOffset = AddressOffset + AddressSize;

bool validDatagramSize(std::size_t size)
{
    return size >= 1 && size <= rtype::network::MaxSealedDatagramSize;
}

} // namespace

std::vector<std::byte> encodeRelayFrame(const rtype::network::Address &client,
                                        std::span<const std::byte> datagram)
{
    if (client.port() == 0) {
        throw std::invalid_argument("The port of a client is never 0");
    }
    if (!validDatagramSize(datagram.size())) {
        throw std::invalid_argument("A relayed datagram is 1 to 1200 bytes");
    }
    std::vector<std::byte> frame(RelayFrameHeaderSize + datagram.size(), std::byte{0});
    frame[0] = static_cast<std::byte>(client.isIpv4() ? FamilyIpv4 : FamilyIpv6);
    const std::vector<std::uint8_t> ip = client.bytes();
    std::ranges::transform(ip, frame.begin() + AddressOffset,
                           [](std::uint8_t byte) { return static_cast<std::byte>(byte); });
    frame[PortOffset] = static_cast<std::byte>(client.port() >> 8);
    frame[PortOffset + 1] = static_cast<std::byte>(client.port() & 0xFF);
    std::ranges::copy(datagram, frame.begin() + RelayFrameHeaderSize);
    return frame;
}

std::optional<RelayFrame> decodeRelayFrame(std::span<const std::byte> frame)
{
    if (frame.size() < RelayFrameHeaderSize || !validDatagramSize(frame.size() - RelayFrameHeaderSize)) {
        return std::nullopt;
    }
    const auto family = static_cast<std::uint8_t>(frame[0]);
    const std::span<const std::byte> ip = frame.subspan(AddressOffset, AddressSize);
    const auto port = static_cast<std::uint16_t>((static_cast<unsigned>(frame[PortOffset]) << 8) |
                                                 static_cast<unsigned>(frame[PortOffset + 1]));
    if (port == 0) {
        return std::nullopt;
    }
    std::optional<rtype::network::Address> client;
    if (family == FamilyIpv4) {
        if (std::ranges::any_of(ip.subspan(4), [](std::byte byte) { return byte != std::byte{0}; })) {
            return std::nullopt;
        }
        std::array<std::uint8_t, 4> bytes{};
        std::ranges::transform(ip.first(4), bytes.begin(),
                               [](std::byte byte) { return static_cast<std::uint8_t>(byte); });
        client = rtype::network::Address::ipv4(bytes, port);
    } else if (family == FamilyIpv6) {
        std::array<std::uint8_t, 16> bytes{};
        std::ranges::transform(ip, bytes.begin(),
                               [](std::byte byte) { return static_cast<std::uint8_t>(byte); });
        client = rtype::network::Address::ipv6(bytes, port);
    } else {
        return std::nullopt;
    }
    return RelayFrame{.client = *client, .datagram = frame.subspan(RelayFrameHeaderSize)};
}

} // namespace rtype::server
