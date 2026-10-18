#pragma once

#include "Common/Secret.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The messages between the master and its instances, on their local connection (docs/protocol/instances.md,
/// section 4). Both sides use them: the instance now, the master with the lobby. Like the game messages, the
/// encoders throw std::invalid_argument for a value the protocol forbids, and the decoders are pure: nothing
/// for a malformed payload, never an exception.
namespace rtype::server
{

/// The version of this protocol, in INSTANCE_HELLO.
inline constexpr std::uint16_t InstanceProtocolVersion = 1;
/// Longest name of a game, in bytes of UTF-8.
inline constexpr std::size_t MaxGameNameLength = 32;
/// Longest name of a player, in bytes of UTF-8.
inline constexpr std::size_t MaxInstancePlayerNameLength = 16;
/// Most players in a game.
inline constexpr std::uint8_t MaxInstancePlayers = 4;

/// The types of the frames: 0x01–0x7F from the instance, 0x80–0xFF from the master.
namespace instance_message
{
inline constexpr std::uint8_t InstanceHello = 0x01;
inline constexpr std::uint8_t Ready = 0x02;
inline constexpr std::uint8_t PlayerConnected = 0x03;
inline constexpr std::uint8_t PlayerDisconnected = 0x04;
inline constexpr std::uint8_t GameOver = 0x05;
inline constexpr std::uint8_t Configure = 0x81;
inline constexpr std::uint8_t AddPlayer = 0x82;
inline constexpr std::uint8_t RemovePlayer = 0x83;
inline constexpr std::uint8_t Shutdown = 0x84;
} // namespace instance_message

// --- Instance to master ---

/// `INSTANCE_HELLO`: the instance proves the master started it.
struct InstanceHelloMessage {
    std::uint16_t version = InstanceProtocolVersion;
    /// The `--id` argument, never 0.
    std::uint32_t id = 0;
    /// The secret read on the standard input.
    Secret secret{};

    friend bool operator==(const InstanceHelloMessage &, const InstanceHelloMessage &) = default;
};

/// `READY`: the UDP port is open.
struct ReadyMessage {
    std::uint16_t udpPort = 0;

    friend bool operator==(const ReadyMessage &, const ReadyMessage &) = default;
};

/// `PLAYER_CONNECTED`: the session of a player received `CONNECT`.
struct PlayerConnectedMessage {
    std::uint32_t playerId = 0;

    friend bool operator==(const PlayerConnectedMessage &, const PlayerConnectedMessage &) = default;
};

/// Why the session of a player closed on the side of the game.
enum class PlayerDisconnectedReason : std::uint8_t {
    /// The player left: `DISCONNECT (QUIT)`.
    Quit = 0,
    /// Nothing received for 5 seconds, or no `CONNECT` within 10 seconds of ADD_PLAYER.
    TimedOut = 1,
};

/// `PLAYER_DISCONNECTED`.
struct PlayerDisconnectedMessage {
    std::uint32_t playerId = 0;
    PlayerDisconnectedReason reason = PlayerDisconnectedReason::Quit;

    friend bool operator==(const PlayerDisconnectedMessage &, const PlayerDisconnectedMessage &) = default;
};

/// The score of a player at the end of a game.
struct PlayerScore {
    std::uint32_t playerId = 0;
    std::uint32_t score = 0;

    friend bool operator==(const PlayerScore &, const PlayerScore &) = default;
};

/// `GAME_OVER`: the scores, just before the instance exits.
struct GameOverReport {
    /// At most 4.
    std::vector<PlayerScore> scores;

    friend bool operator==(const GameOverReport &, const GameOverReport &) = default;
};

// --- Master to instance ---

/// `CONFIGURE`: the game to run.
struct ConfigureMessage {
    /// 1 to MaxGameNameLength bytes.
    std::string name;
    /// 1 to 4.
    std::uint8_t maxPlayers = MaxInstancePlayers;

    friend bool operator==(const ConfigureMessage &, const ConfigureMessage &) = default;
};

/// `ADD_PLAYER`: a player who will connect, with the keys of its session.
struct AddPlayerMessage {
    std::uint32_t playerId = 0;
    /// Unique in this instance, never 0.
    std::uint32_t sessionId = 0;
    /// The keys of the datagrams of the session (docs/protocol/game.md, section 3). Secret: never logged.
    std::array<std::byte, 32> clientKey{};
    std::array<std::byte, 32> serverKey{};
    /// 1 to MaxInstancePlayerNameLength bytes.
    std::string name;

    friend bool operator==(const AddPlayerMessage &, const AddPlayerMessage &) = default;
};

/// Why the master removes a player.
enum class RemovePlayerReason : std::uint8_t {
    /// The player sent `LEAVE_GAME`.
    Left = 0,
    /// The connection of the player to the master closed.
    LobbyClosed = 1,
};

/// `REMOVE_PLAYER`.
struct RemovePlayerMessage {
    std::uint32_t playerId = 0;
    RemovePlayerReason reason = RemovePlayerReason::Left;

    friend bool operator==(const RemovePlayerMessage &, const RemovePlayerMessage &) = default;
};

/// Why the master stops a game.
enum class ShutdownReason : std::uint8_t {
    /// The game has no player left.
    Empty = 0,
    /// The master stops.
    MasterStopping = 1,
};

/// `SHUTDOWN`.
struct ShutdownMessage {
    ShutdownReason reason = ShutdownReason::Empty;

    friend bool operator==(const ShutdownMessage &, const ShutdownMessage &) = default;
};

/// The payloads of the messages.
/// @throws std::invalid_argument for a value the protocol forbids (an id of 0, a name too long...).
[[nodiscard]] std::vector<std::byte> encode(const InstanceHelloMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ReadyMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerConnectedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerDisconnectedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameOverReport &message);
[[nodiscard]] std::vector<std::byte> encode(const ConfigureMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const AddPlayerMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const RemovePlayerMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ShutdownMessage &message);

/// The messages read from their payloads, strictly: master and instances come from the same build, so an
/// unknown reason is malformed too.
[[nodiscard]] std::optional<InstanceHelloMessage> decodeInstanceHello(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ReadyMessage> decodeReady(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerConnectedMessage> decodePlayerConnected(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerDisconnectedMessage>
decodePlayerDisconnected(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameOverReport> decodeGameOverReport(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ConfigureMessage> decodeConfigure(std::span<const std::byte> payload);
[[nodiscard]] std::optional<AddPlayerMessage> decodeAddPlayer(std::span<const std::byte> payload);
[[nodiscard]] std::optional<RemovePlayerMessage> decodeRemovePlayer(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ShutdownMessage> decodeShutdown(std::span<const std::byte> payload);

} // namespace rtype::server
