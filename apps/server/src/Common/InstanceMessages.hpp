#pragma once

#include "Common/Secret.hpp"
#include "rtype/voice/Format.hpp"

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
inline constexpr std::uint16_t InstanceProtocolVersion = 7;
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
inline constexpr std::uint8_t GameStatus = 0x06;
inline constexpr std::uint8_t Configure = 0x81;
inline constexpr std::uint8_t AddPlayer = 0x82;
inline constexpr std::uint8_t RemovePlayer = 0x83;
inline constexpr std::uint8_t Shutdown = 0x84;
inline constexpr std::uint8_t MuteVoice = 0x85;
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
    /// Where the game receives: its public port without a relay, its port on the loopback behind one, where
    /// the relay sends its frames (version 4).
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

/// The score of a player at the end of a game, and what it earned (version 5).
struct PlayerScore {
    std::uint32_t playerId = 0;
    std::uint32_t score = 0;
    std::uint32_t kills = 0;
    std::uint16_t wavesCleared = 0;
    std::uint16_t bestStreak = 0;
    /// The experience earned in the game.
    std::uint32_t xp = 0;

    friend bool operator==(const PlayerScore &, const PlayerScore &) = default;
};

/// `GAME_OVER`: the scores, just before the instance exits.
struct GameOverReport {
    /// At most 4.
    std::vector<PlayerScore> scores;

    friend bool operator==(const GameOverReport &, const GameOverReport &) = default;
};

/// A player in a GAME_STATUS.
struct PlayerStatus {
    std::uint32_t playerId = 0;
    std::uint32_t score = 0;
    /// Lives left: false once out of the game.
    bool alive = false;
    /// The round-trip time to its client, in milliseconds, at most MaxStatusLatency; nothing until measured.
    std::optional<std::uint16_t> latency;

    friend bool operator==(const PlayerStatus &, const PlayerStatus &) = default;
};

/// The highest latency a GAME_STATUS carries: 0xFFFF on the wire means "not measured yet".
inline constexpr std::uint16_t MaxStatusLatency = 0xFFFE;

/// `GAME_STATUS`: where the game stands, every second (version 2).
struct GameStatusMessage {
    /// The last tick simulated.
    std::uint32_t tick = 0;
    /// The wave under way, from 1; 0 before the first.
    std::uint16_t wave = 0;
    /// The players whose client connected, at most 4, each id once.
    std::vector<PlayerStatus> players;

    friend bool operator==(const GameStatusMessage &, const GameStatusMessage &) = default;
};

// --- Master to instance ---

/// What the game does with the voice, from `[voice]` of the settings of the master (version 7).
struct VoiceRules {
    /// Whether the game carries the voice: without it, every VOICE is dropped and ACCEPTED tells the client.
    bool enabled = true;
    /// The largest frame forwarded, 1 to voice::MaxFrameBytes.
    std::uint8_t maxFrameBytes = static_cast<std::uint8_t>(rtype::voice::MaxFrameBytes);
    /// The frames of one player forwarded per second in the long run, never 0. A voice is 50.
    std::uint8_t maxFramesPerSecond = 60;

    friend bool operator==(const VoiceRules &, const VoiceRules &) = default;
};

/// How long the voice of a player is muted by an administrator, in seconds: 0 not muted, this value until
/// the end of the game.
inline constexpr std::uint32_t VoiceMutedForTheGame = 0xFFFFFFFF;

/// `CONFIGURE`: the game to run.
struct ConfigureMessage {
    /// 1 to MaxGameNameLength bytes.
    std::string name;
    /// 1 to 4.
    std::uint8_t maxPlayers = MaxInstancePlayers;
    /// The port of the relay of the master, on 127.0.0.1 (version 4): the game takes its datagrams from it in
    /// relay frames (Common/RelayFrame.hpp). 0: no relay, the game opens its own port, `--udp-port`.
    std::uint16_t relayPort = 0;
    /// The voice of the game (version 7).
    VoiceRules voice{};

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
    /// The id of its account, 0 for a guest (version 6): told to the other players in PLAYER_JOINED.
    std::uint64_t accountId = 0;
    /// Its account is muted by an administrator for this many seconds more (version 7), 0 if not: its voice
    /// is dropped until then.
    std::uint32_t voiceMuted = 0;

    friend bool operator==(const AddPlayerMessage &, const AddPlayerMessage &) = default;
};

/// Why the master removes a player.
enum class RemovePlayerReason : std::uint8_t {
    /// The player sent `LEAVE_GAME`.
    Left = 0,
    /// The connection of the player to the master closed.
    LobbyClosed = 1,
    /// An administrator removed the player (version 3).
    Kicked = 2,
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
    /// An administrator ended the game (version 3).
    EndedByAdmin = 2,
};

/// `SHUTDOWN`.
struct ShutdownMessage {
    ShutdownReason reason = ShutdownReason::Empty;

    friend bool operator==(const ShutdownMessage &, const ShutdownMessage &) = default;
};

/// `MUTE_VOICE` (version 7): an administrator mutes the voice of a player, or lets it talk again.
struct MuteVoiceMessage {
    std::uint32_t playerId = 0;
    /// Seconds, from now: 0 lets it talk, VoiceMutedForTheGame until the end of the game.
    std::uint32_t duration = 0;

    friend bool operator==(const MuteVoiceMessage &, const MuteVoiceMessage &) = default;
};

/// The payloads of the messages.
/// @throws std::invalid_argument for a value the protocol forbids (an id of 0, a name too long...).
[[nodiscard]] std::vector<std::byte> encode(const InstanceHelloMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ReadyMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerConnectedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerDisconnectedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameOverReport &message);
[[nodiscard]] std::vector<std::byte> encode(const GameStatusMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ConfigureMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const AddPlayerMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const RemovePlayerMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ShutdownMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const MuteVoiceMessage &message);

/// The messages read from their payloads, strictly: master and instances come from the same build, so an
/// unknown reason is malformed too.
[[nodiscard]] std::optional<InstanceHelloMessage> decodeInstanceHello(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ReadyMessage> decodeReady(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerConnectedMessage> decodePlayerConnected(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerDisconnectedMessage>
decodePlayerDisconnected(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameOverReport> decodeGameOverReport(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameStatusMessage> decodeGameStatus(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ConfigureMessage> decodeConfigure(std::span<const std::byte> payload);
[[nodiscard]] std::optional<AddPlayerMessage> decodeAddPlayer(std::span<const std::byte> payload);
[[nodiscard]] std::optional<RemovePlayerMessage> decodeRemovePlayer(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ShutdownMessage> decodeShutdown(std::span<const std::byte> payload);
[[nodiscard]] std::optional<MuteVoiceMessage> decodeMuteVoice(std::span<const std::byte> payload);

} // namespace rtype::server
