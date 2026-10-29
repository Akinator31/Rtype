#include "Master/AddressBans.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace rtype::server
{

rtype::network::Address banKey(const rtype::network::Address &address)
{
    if (address.isIpv6()) {
        const std::vector<std::uint8_t> bytes = address.bytes();
        const bool mapped =
            std::all_of(bytes.begin(), bytes.begin() + 10, [](std::uint8_t b) { return b == 0; }) &&
            bytes[10] == 0xFF && bytes[11] == 0xFF;
        if (mapped) {
            return rtype::network::Address::ipv4({bytes[12], bytes[13], bytes[14], bytes[15]}, 0);
        }
    }
    return address.withPort(0);
}

AddressBans::AddressBans(const std::vector<AddressBan> &bans)
{
    for (const AddressBan &ban : bans) {
        add(ban);
    }
}

std::optional<AddressBan> AddressBans::find(const rtype::network::Address &address,
                                            std::chrono::system_clock::time_point now) const
{
    const auto found = _bans.find(banKey(address));
    if (found == _bans.end() || found->second.expiresAt <= now) {
        return std::nullopt;
    }
    return found->second;
}

void AddressBans::add(AddressBan ban)
{
    ban.address = banKey(ban.address);
    const rtype::network::Address key = ban.address;
    _bans.insert_or_assign(key, std::move(ban));
}

std::size_t AddressBans::purge(std::chrono::system_clock::time_point now)
{
    return std::erase_if(_bans, [now](const auto &entry) { return entry.second.expiresAt <= now; });
}

std::vector<AddressBan> AddressBans::list(std::chrono::system_clock::time_point now) const
{
    std::vector<AddressBan> bans;
    for (const auto &[address, ban] : _bans) {
        if (ban.expiresAt > now) {
            bans.push_back(ban);
        }
    }
    std::ranges::sort(bans, [](const AddressBan &first, const AddressBan &second) {
        return first.expiresAt != second.expiresAt ? first.expiresAt < second.expiresAt
                                                   : first.address.toString() < second.address.toString();
    });
    return bans;
}

} // namespace rtype::server
