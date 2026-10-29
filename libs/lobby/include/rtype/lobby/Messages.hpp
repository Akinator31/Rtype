#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// The messages between a client and the master, in the frames of their TLS connection
/// (docs/protocol/lobby.md, section 5). The client and the server share them: the client writes the requests
/// and reads the replies, the master the other way round.
///
/// @code
/// // Client: HELLO may be sent at once, it waits for the end of the handshake
/// const rtype::lobby::HelloMessage hello{.name = "Pavel"};
/// connection.send(rtype::lobby::message_type::Hello, rtype::lobby::encode(hello));
///
/// // Client, in the frame handler
/// if (type == rtype::lobby::message_type::Welcome) {
///     if (const auto welcome = rtype::lobby::decodeWelcome(payload)) { playerId = welcome->playerId; }
/// }
/// @endcode
///
/// The encoders throw std::invalid_argument for a value the protocol forbids. The decoders are pure and never
/// throw: nothing for a malformed payload. Two fields are read as they come, and checked by the master
/// afterwards, since a wrong value there is not malformed but has its own error: the version of HELLO
/// (UNSUPPORTED_VERSION), and the names of HELLO and CREATE_GAME (INVALID_NAME, see isValidPlayerName()).
namespace rtype::lobby
{

/// The version of the lobby protocol, in HELLO.
inline constexpr std::uint16_t ProtocolVersion = 2;
/// Longest name of a player, in bytes of UTF-8.
inline constexpr std::size_t MaxPlayerNameLength = 16;
/// Longest name of a game, in bytes of UTF-8.
inline constexpr std::size_t MaxGameNameLength = 32;
/// Most players in a game.
inline constexpr std::uint8_t MaxPlayers = 4;
/// Most games in a GAME_LIST.
inline constexpr std::size_t MaxGamesInList = 256;
/// Longest text of a SERVER_MESSAGE, in bytes of UTF-8.
inline constexpr std::size_t MaxServerMessageLength = 200;

/// The types of the frames: 0x01–0x7F from the client, 0x80–0xFF from the master.
namespace message_type
{
inline constexpr std::uint8_t Hello = 0x01;
inline constexpr std::uint8_t ListGames = 0x02;
inline constexpr std::uint8_t CreateGame = 0x03;
inline constexpr std::uint8_t JoinGame = 0x04;
inline constexpr std::uint8_t LeaveGame = 0x05;
inline constexpr std::uint8_t Ping = 0x06;
inline constexpr std::uint8_t Welcome = 0x81;
inline constexpr std::uint8_t GameList = 0x82;
inline constexpr std::uint8_t GameJoined = 0x83;
inline constexpr std::uint8_t LeftGame = 0x84;
inline constexpr std::uint8_t Pong = 0x85;
inline constexpr std::uint8_t Error = 0x86;
inline constexpr std::uint8_t ServerMessage = 0x87;
} // namespace message_type

/// Tells whether a name can be the name of a player: 1 to 16 bytes of UTF-8, without control characters.
[[nodiscard]] bool isValidPlayerName(std::string_view name);
/// Tells whether a name can be the name of a game: 1 to 32 bytes of UTF-8, without control characters.
[[nodiscard]] bool isValidGameName(std::string_view name);
/// Tells whether a text can be the text of a SERVER_MESSAGE: 1 to 200 bytes of UTF-8, without control
/// characters.
[[nodiscard]] bool isValidServerMessage(std::string_view text);

// --- Client to master ---

/// `HELLO`: the first message of a client.
struct HelloMessage {
    /// Decoded as sent: the master answers UNSUPPORTED_VERSION if it is not ProtocolVersion.
    std::uint16_t version = ProtocolVersion;
    /// Decoded as sent: the master answers INVALID_NAME if !isValidPlayerName().
    std::string name;

    friend bool operator==(const HelloMessage &, const HelloMessage &) = default;
};

/// `CREATE_GAME`.
struct CreateGameMessage {
    /// Decoded as sent: the master answers INVALID_NAME if !isValidGameName().
    std::string name;
    /// 1 to 4: anything else is malformed.
    std::uint8_t maxPlayers = MaxPlayers;

    friend bool operator==(const CreateGameMessage &, const CreateGameMessage &) = default;
};

/// `JOIN_GAME`.
struct JoinGameMessage {
    /// An id from GAME_LIST.
    std::uint32_t gameId = 0;

    friend bool operator==(const JoinGameMessage &, const JoinGameMessage &) = default;
};

// --- Master to client ---

/// `WELCOME`: the answer to HELLO.
struct WelcomeMessage {
    /// The id of the player on this master, never 0.
    std::uint32_t playerId = 0;

    friend bool operator==(const WelcomeMessage &, const WelcomeMessage &) = default;
};

/// One game of GAME_LIST.
struct GameInfo {
    /// Never 0.
    std::uint32_t id = 0;
    /// 1 to 32 bytes.
    std::string name;
    /// Players in the game now.
    std::uint8_t players = 0;
    /// 1 to 4.
    std::uint8_t maxPlayers = MaxPlayers;

    friend bool operator==(const GameInfo &, const GameInfo &) = default;
};

/// `GAME_LIST`: the answer to LIST_GAMES.
struct GameListMessage {
    /// At most MaxGamesInList, sorted by id, each id once.
    std::vector<GameInfo> games;

    friend bool operator==(const GameListMessage &, const GameListMessage &) = default;
};

/// `GAME_JOINED`: the answer to CREATE_GAME and JOIN_GAME, with what the client needs to open its UDP
/// session.
struct GameJoinedMessage {
    std::uint32_t gameId = 0;
    /// The UDP port of the game, on the IP address of the master.
    std::uint16_t udpPort = 0;
    /// Never 0.
    std::uint32_t sessionId = 0;
    /// The keys of the datagrams. Secret: never logged.
    std::array<std::byte, 32> clientKey{};
    std::array<std::byte, 32> serverKey{};

    friend bool operator==(const GameJoinedMessage &, const GameJoinedMessage &) = default;
};

/// Why a player is no longer in its game.
enum class LeftGameReason : std::uint8_t {
    /// The answer to LEAVE_GAME.
    Requested = 0,
    /// The game ended normally.
    GameOver = 1,
    /// The game process stopped unexpectedly.
    GameCrashed = 2,
    /// The game received nothing from the client for 5 seconds.
    TimedOut = 3,
    /// An administrator removed the player: ERROR (KICKED) follows (version 2).
    Kicked = 4,
    /// An administrator ended the game (version 2).
    EndedByAdmin = 5,
};

/// `LEFT_GAME`: the answer to LEAVE_GAME, or a notification.
struct LeftGameMessage {
    std::uint32_t gameId = 0;
    LeftGameReason reason = LeftGameReason::Requested;

    friend bool operator==(const LeftGameMessage &, const LeftGameMessage &) = default;
};

/// The code of an ERROR. A client shows the codes it does not know as a generic error, so any value is
/// decoded.
enum class ErrorCode : std::uint16_t {
    /// Invalid frame or field: the connection is closed after the error.
    Malformed = 1,
    UnknownMessage = 2,
    /// The connection is closed after the error.
    UnsupportedVersion = 3,
    HelloRequired = 4,
    AlreadyWelcomed = 5,
    InvalidName = 6,
    GameNotFound = 7,
    GameFull = 8,
    AlreadyInGame = 9,
    NotInGame = 10,
    ServerFull = 11,
    GameStartFailed = 12,
    /// An administrator removed the player: sent without a request, then the connection is closed (version
    /// 2).
    Kicked = 13,
    /// The address of the client is banned: the answer to HELLO, or sent without a request when the ban comes
    /// later; the connection is closed after it (version 2).
    Banned = 14,
};

/// Tells whether the master closes the connection after this error: MALFORMED, UNSUPPORTED_VERSION, KICKED
/// and BANNED.
[[nodiscard]] bool closesConnection(ErrorCode code);

/// `ERROR`: the answer to a request that cannot be honoured, or, for KICKED and BANNED, a notification.
struct ErrorMessage {
    /// The type of the request that failed, 0 if it could not be read or if the error answers no request.
    std::uint8_t request = 0;
    ErrorCode code = ErrorCode::Malformed;

    friend bool operator==(const ErrorMessage &, const ErrorMessage &) = default;
};

/// `SERVER_MESSAGE`: a notification, a text the administrators of the server send (version 2).
struct ServerMessageMessage {
    /// 1 to MaxServerMessageLength bytes of text.
    std::string text;

    friend bool operator==(const ServerMessageMessage &, const ServerMessageMessage &) = default;
};

/// The payloads. LIST_GAMES, LEAVE_GAME, PING and PONG have none: send an empty payload.
/// @throws std::invalid_argument for a value the protocol forbids.
[[nodiscard]] std::vector<std::byte> encode(const HelloMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const CreateGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const JoinGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const WelcomeMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameListMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameJoinedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const LeftGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ErrorMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ServerMessageMessage &message);

[[nodiscard]] std::optional<HelloMessage> decodeHello(std::span<const std::byte> payload);
[[nodiscard]] std::optional<CreateGameMessage> decodeCreateGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<JoinGameMessage> decodeJoinGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<WelcomeMessage> decodeWelcome(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameListMessage> decodeGameList(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameJoinedMessage> decodeGameJoined(std::span<const std::byte> payload);
[[nodiscard]] std::optional<LeftGameMessage> decodeLeftGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ErrorMessage> decodeError(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ServerMessageMessage> decodeServerMessage(std::span<const std::byte> payload);
/// For the messages without payload: true if it is empty.
[[nodiscard]] bool decodeEmpty(std::span<const std::byte> payload);

} // namespace rtype::lobby
