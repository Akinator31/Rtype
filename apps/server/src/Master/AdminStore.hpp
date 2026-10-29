#pragma once

#include "Master/AddressBans.hpp"
#include "rtype/database/Database.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// What the administration keeps in the database of the master (docs/architecture/administration.md): the
/// record of the actions (`admin_actions`) and the bans of addresses (`address_bans`). Plain functions on a
/// Database: the master runs them on its database thread, the tests anywhere.
namespace rtype::server
{

/// The actor of an action done with the local access token, until the accounts of step 12.
inline constexpr std::string_view LocalActor = "local";

/// One action of an administrator (table `admin_actions`).
struct AdminAction {
    /// 0 until recorded.
    long long id = 0;
    std::chrono::system_clock::time_point at;
    /// `local`, an account id, or `deleted`.
    std::string actor;
    /// `kick`, `end_game`, `message` or `ban`.
    std::string action;
    /// What it was done to: `player:7`, `game:3`, `everyone`, `address:203.0.113.9`.
    std::string target;
    /// A JSON object: the name of the player, the text of the message, the reason...
    std::string details = "{}";

    friend bool operator==(const AdminAction &, const AdminAction &) = default;
};

/// Unix milliseconds, as the database keeps dates.
[[nodiscard]] long long unixMilliseconds(std::chrono::system_clock::time_point time);
[[nodiscard]] std::chrono::system_clock::time_point fromUnixMilliseconds(long long milliseconds);

/// Records an action.
/// @return Its id: the ids grow, never reused.
/// @throws soci::soci_error if the database refuses it.
long long recordAction(rtype::database::Database &database, const AdminAction &action);

/// The actions recorded, newest first.
/// @param limit How many at most.
/// @param before Only those with a smaller id: the page after one ending at this id.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::vector<AdminAction> readActions(rtype::database::Database &database, std::size_t limit,
                                                   std::optional<long long> before = std::nullopt);

/// Keeps a ban, replacing the one of its address.
/// @throws soci::soci_error if the database refuses it.
void saveAddressBan(rtype::database::Database &database, const AddressBan &ban);

/// Removes the ban of an address, lifted before it expires.
/// @throws soci::soci_error if the database refuses it.
void deleteAddressBan(rtype::database::Database &database, const rtype::network::Address &address);

/// Removes the bans expired at `now`.
/// @return How many.
/// @throws soci::soci_error if the database refuses it.
std::size_t purgeAddressBans(rtype::database::Database &database, std::chrono::system_clock::time_point now);

/// The bans that still run at `now`, the expired ones purged first: what the master reads at start.
/// @throws soci::soci_error if the database cannot be read.
[[nodiscard]] std::vector<AddressBan> loadAddressBans(rtype::database::Database &database,
                                                      std::chrono::system_clock::time_point now);

} // namespace rtype::server
