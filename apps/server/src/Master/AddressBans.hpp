#pragma once

#include "rtype/network/Address.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtype::server
{

/// The longest ban of an address: addresses are shared and change, so a ban is temporary
/// (docs/architecture/administration.md#bans).
inline constexpr std::chrono::hours MaxBanDuration{7 * 24};

/// An IP address the lobby refuses until a date (table `address_bans`).
struct AddressBan {
    /// Its port 0, an IPv4 address written in IPv6 (`::ffff:1.2.3.4`) as IPv4.
    rtype::network::Address address;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point expiresAt;
    std::string reason;
    /// Who banned it: `local` for the local access token.
    std::string actor;

    friend bool operator==(const AddressBan &, const AddressBan &) = default;
};

/// The address a ban is about: its port 0, and an IPv4 address written in IPv6 as the IPv4 address it holds,
/// so that a listener on `[::]` sees the same address as one on `0.0.0.0`.
[[nodiscard]] rtype::network::Address banKey(const rtype::network::Address &address);

/// The bans of the master, in memory, checked by the lobby at HELLO. The database keeps them across restarts:
/// they are read from it at start (loadAddressBans()), and written to it as they are added. On the thread of
/// the loop.
class AddressBans
{
    std::unordered_map<rtype::network::Address, AddressBan> _bans;

public:
    explicit AddressBans(const std::vector<AddressBan> &bans = {});

    /// The ban of an address, if one runs at `now`.
    [[nodiscard]] std::optional<AddressBan>
    find(const rtype::network::Address &address,
         std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
    /// Adds a ban, or replaces the one of its address.
    void add(AddressBan ban);
    /// Forgets the bans expired at `now`.
    /// @return How many.
    std::size_t purge(std::chrono::system_clock::time_point now);
    /// The bans that run at `now`, the first to expire first.
    [[nodiscard]] std::vector<AddressBan>
    list(std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
};

} // namespace rtype::server
