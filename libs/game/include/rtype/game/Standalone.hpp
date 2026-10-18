#pragma once

#include "rtype/game/Constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

/// The sessions of a game in standalone mode: `r-type_server --instance --standalone`, in debug builds only,
/// a game without a master (ADR 0013). Without a master, nobody gives the sessions and their keys to the
/// clients, so they are fixed, here, for the server and for a client in development.
///
/// @code
/// rtype::network::ClientSession session(rtype::game::standalone::sessionId(0),
///                                       rtype::network::DatagramKey(rtype::game::standalone::clientKey(0)),
///                                       rtype::network::DatagramKey(rtype::game::standalone::serverKey(0)),
///                                       send);
/// @endcode
///
/// These keys are public: anyone who reads this file can join or read a standalone game. A released server
/// refuses the standalone mode.
namespace rtype::game::standalone
{

using Key = std::array<std::byte, 32>;

/// The session of the slot `slot`, 0 to 3: sessions 1 to 4, each one for one player.
[[nodiscard]] constexpr std::uint32_t sessionId(std::uint8_t slot)
{
    return static_cast<std::uint32_t>(slot) + 1;
}

/// The key of the datagrams sent by the client of a session: every byte `0x10 + slot`.
[[nodiscard]] constexpr Key clientKey(std::uint8_t slot)
{
    Key key{};
    key.fill(static_cast<std::byte>(0x10 + slot));
    return key;
}

/// The key of the datagrams sent by the game to a session: every byte `0x20 + slot`.
[[nodiscard]] constexpr Key serverKey(std::uint8_t slot)
{
    Key key{};
    key.fill(static_cast<std::byte>(0x20 + slot));
    return key;
}

} // namespace rtype::game::standalone
