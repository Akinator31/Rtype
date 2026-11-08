#pragma once

#include <cstdint>
#include <string_view>

/// What carries the voice of each player of a game (docs/architecture/voice.md, ADR 0032).
namespace rtype::voice
{

/// What a player talks and hears on. The values are those of the game protocol (`VOICE_STATE`).
enum class Backend : std::uint8_t {
    /// No voice: voice off, no microphone and no output, or the game has no voice.
    None = 0,
    /// The voice of the game, in its UDP session.
    Game = 1,
    /// The voice of Discord, in the lobby of the game.
    Discord = 2,
};

/// The name of a backend, for the logs: "none", "game" or "discord".
[[nodiscard]] constexpr std::string_view name(Backend backend)
{
    switch (backend) {
    case Backend::Game:
        return "game";
    case Backend::Discord:
        return "discord";
    case Backend::None:
        break;
    }
    return "none";
}

/// Whether the game carries the voice of a player on `from` to a player on `to`. Two players on Discord
/// already hear each other there; a player on None neither talks nor hears.
///
/// @code
/// carriedByGame(Backend::Discord, Backend::Game);    // true: the game is the only path between them
/// carriedByGame(Backend::Discord, Backend::Discord); // false: Discord carries it, the game would double it
/// @endcode
[[nodiscard]] constexpr bool carriedByGame(Backend from, Backend to)
{
    if (from == Backend::None || to == Backend::None) {
        return false;
    }
    return from == Backend::Game || to == Backend::Game;
}

} // namespace rtype::voice
