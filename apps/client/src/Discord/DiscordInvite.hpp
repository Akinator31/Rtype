#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// Joining a game online from Discord (docs/architecture/client-online.md#invitations): the activity of a
/// player in a game online carries a join secret, which Discord gives to the friends it invites, or who ask
/// to join and are accepted. The game of that friend connects to the server, then joins the game.
namespace rtype::client
{

/// Discord keeps join secrets and party ids of at most 128 bytes.
inline constexpr std::size_t MaxDiscordSecretLength = 128;

/// Where a friend can be invited: the server as KnownServers knows it ("host:port", serverAddress()), and the
/// id of the game on it (GAME_LIST, JOIN_GAME), none for the lobby of the server, its list of games.
struct GameInvite {
    std::string server;
    std::optional<std::uint32_t> gameId = {};

    friend bool operator==(const GameInvite &, const GameInvite &) = default;
};

/// The join secret of a game: "rtype1:<game id>:<server>", "rtype1::<server>" for the lobby of the server. It
/// tells nothing the server does not tell every player: its games are listed to all. None when the server is
/// too long for Discord.
[[nodiscard]] std::optional<std::string> inviteSecret(const GameInvite &invite);
/// The game of a join secret, none if it is not one: the secret comes from Discord, from another player.
[[nodiscard]] std::optional<GameInvite> parseInviteSecret(std::string_view secret);
/// The id of the party of a game, the same for all its players, or of the lobby of a server: a hash of the
/// server and the game, which tells neither.
[[nodiscard]] std::string partyId(const GameInvite &invite);

} // namespace rtype::client
