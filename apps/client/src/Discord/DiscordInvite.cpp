#include "Discord/DiscordInvite.hpp"

#include "Online/OnlineScenes.hpp"

#include <charconv>
#include <format>
#include <system_error>

namespace rtype::client
{

namespace
{

constexpr std::string_view SecretPrefix = "rtype1:";

} // namespace

std::optional<std::string> inviteSecret(const GameInvite &invite)
{
    const std::string game = invite.gameId ? std::to_string(*invite.gameId) : std::string();
    std::string secret = std::format("{}{}:{}", SecretPrefix, game, invite.server);
    if (invite.server.empty() || secret.size() > MaxDiscordSecretLength) {
        return std::nullopt;
    }
    return secret;
}

std::optional<GameInvite> parseInviteSecret(std::string_view secret)
{
    if (secret.size() > MaxDiscordSecretLength || !secret.starts_with(SecretPrefix)) {
        return std::nullopt;
    }
    secret.remove_prefix(SecretPrefix.size());
    const std::size_t colon = secret.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    GameInvite invite;
    // No game: the lobby of the server
    if (colon > 0) {
        std::uint32_t gameId = 0;
        const auto [end, error] = std::from_chars(secret.data(), secret.data() + colon, gameId);
        if (error != std::errc() || end != secret.data() + colon) {
            return std::nullopt;
        }
        invite.gameId = gameId;
    }
    invite.server = std::string(secret.substr(colon + 1));
    // Printable text only, and a server the Online menu could connect to
    for (const char character : invite.server) {
        if (static_cast<unsigned char>(character) <= ' ' || static_cast<unsigned char>(character) >= 0x7f) {
            return std::nullopt;
        }
    }
    if (!splitServer(invite.server)) {
        return std::nullopt;
    }
    return invite;
}

std::string partyId(const GameInvite &invite)
{
    // FNV-1a, 64 bits: the same in every client, whatever its platform
    std::uint64_t hash = 14695981039346656037ULL;
    const auto mix = [&hash](std::string_view text) {
        for (const char character : text) {
            hash = (hash ^ static_cast<unsigned char>(character)) * 1099511628211ULL;
        }
    };
    mix(invite.server);
    mix("#");
    mix(invite.gameId ? std::to_string(*invite.gameId) : std::string("lobby"));
    return std::format("rtype-{:016x}", hash);
}

} // namespace rtype::client
