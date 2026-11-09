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
/// const rtype::lobby::HelloMessage hello{.name = "Pavel", .locale = "fr"};
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
///
/// The profiles (version 4): AccountInfo carries what the account knows of itself, GET_PROFILE the Discord
/// profile of an account, GET_AVATAR its picture, in parts that fit in a frame.
///
/// The fields of the accounts (version 3) are decoded within their lengths; what they hold is checked by the
/// master, which has its own error for each: INVALID_NAME for a username, INVALID_EMAIL, WEAK_PASSWORD. A
/// password, a session token and a code are secrets: never logged, at any level.
namespace rtype::lobby
{

/// The version of the lobby protocol, in HELLO: 7 gives the players with Discord the secret of the voice
/// lobby of their game (GAME_JOINED).
inline constexpr std::uint16_t ProtocolVersion = 7;
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
/// Shortest and longest locale of HELLO, such as `en`, `fr-FR` or `fr_CA`.
inline constexpr std::size_t MinLocaleLength = 2;
inline constexpr std::size_t MaxLocaleLength = 16;
/// Longest username of an account, in bytes: the master allows 3 to 16 of `A-Z a-z 0-9 _ -`.
inline constexpr std::size_t MaxUsernameLength = 16;
/// Longest email, in bytes: the longest address SMTP carries. A username or an email, when either is
/// accepted, has this bound too.
inline constexpr std::size_t MaxEmailLength = 254;
/// Longest password, in bytes: 128 characters of up to 4 bytes. The master allows 10 to 128 characters.
inline constexpr std::size_t MaxPasswordLength = 512;
/// The digits of a code sent by email.
inline constexpr std::size_t CodeLength = 6;
/// Longest authorization code of Discord in DISCORD_CONFIRM, in bytes.
inline constexpr std::size_t MaxDiscordCodeLength = 255;
/// Shortest and longest code verifier of DISCORD_CONFIRM: those of PKCE (RFC 7636, section 4.1).
inline constexpr std::size_t MinCodeVerifierLength = 43;
inline constexpr std::size_t MaxCodeVerifierLength = 128;
/// Longest redirect address of DISCORD_CONFIRM, in bytes.
inline constexpr std::size_t MaxRedirectUriLength = 2048;
/// Longest name of a Discord user (`@username`), display name, and version of a picture, in PROFILE, in
/// bytes.
inline constexpr std::size_t MaxDiscordUsernameLength = 64;
inline constexpr std::size_t MaxDisplayNameLength = 128;
inline constexpr std::size_t MaxAvatarHashLength = 64;
/// Longest scopes of DISCORD_AUTHORIZE, and longest token of Discord in DISCORD_TOKEN and DISCORD_REFRESH, in
/// bytes (version 6).
inline constexpr std::size_t MaxDiscordScopesLength = 255;
inline constexpr std::size_t MaxDiscordTokenLength = 1024;
/// Largest picture of an account, in bytes, and largest part of it in one AVATAR.
inline constexpr std::size_t MaxAvatarSize = 256 * 1024;
inline constexpr std::size_t MaxAvatarChunk = 16000;

/// A session token: 256 random bits, given by SIGNED_IN, sent back in HELLO. Secret: never logged.
using SessionToken = std::array<std::byte, 32>;

/// The types of the frames: 0x01–0x7F from the client, 0x80–0xFF from the master.
namespace message_type
{
inline constexpr std::uint8_t Hello = 0x01;
inline constexpr std::uint8_t ListGames = 0x02;
inline constexpr std::uint8_t CreateGame = 0x03;
inline constexpr std::uint8_t JoinGame = 0x04;
inline constexpr std::uint8_t LeaveGame = 0x05;
inline constexpr std::uint8_t Ping = 0x06;
inline constexpr std::uint8_t Register = 0x07;
inline constexpr std::uint8_t VerifyEmail = 0x08;
inline constexpr std::uint8_t ResendCode = 0x09;
inline constexpr std::uint8_t SignIn = 0x0A;
inline constexpr std::uint8_t SignOut = 0x0B;
inline constexpr std::uint8_t ForgotPassword = 0x0C;
inline constexpr std::uint8_t ResetPassword = 0x0D;
inline constexpr std::uint8_t DiscordSignIn = 0x0E;
inline constexpr std::uint8_t DiscordConfirm = 0x0F;
inline constexpr std::uint8_t ChooseUsername = 0x10;
inline constexpr std::uint8_t DeleteAccount = 0x11;
inline constexpr std::uint8_t GetProfile = 0x12;
inline constexpr std::uint8_t GetAvatar = 0x13;
inline constexpr std::uint8_t DiscordRefresh = 0x14;
inline constexpr std::uint8_t Welcome = 0x81;
inline constexpr std::uint8_t GameList = 0x82;
inline constexpr std::uint8_t GameJoined = 0x83;
inline constexpr std::uint8_t LeftGame = 0x84;
inline constexpr std::uint8_t Pong = 0x85;
inline constexpr std::uint8_t Error = 0x86;
inline constexpr std::uint8_t ServerMessage = 0x87;
inline constexpr std::uint8_t SignedIn = 0x88;
inline constexpr std::uint8_t CodeSent = 0x89;
inline constexpr std::uint8_t DiscordAuthorize = 0x8A;
inline constexpr std::uint8_t Done = 0x8B;
inline constexpr std::uint8_t Profile = 0x8C;
inline constexpr std::uint8_t Avatar = 0x8D;
inline constexpr std::uint8_t Progress = 0x8E;
inline constexpr std::uint8_t DiscordToken = 0x8F;
} // namespace message_type

/// Tells whether a request is one of the accounts (version 3): REGISTER to DELETE_ACCOUNT, GET_PROFILE and
/// GET_AVATAR (version 4), and DISCORD_REFRESH (version 6).
[[nodiscard]] bool isAccountRequest(std::uint8_t type);

/// Tells whether a name can be the name of a player: 1 to 16 bytes of UTF-8, without control characters.
[[nodiscard]] bool isValidPlayerName(std::string_view name);
/// Tells whether a name can be the name of a game: 1 to 32 bytes of UTF-8, without control characters.
[[nodiscard]] bool isValidGameName(std::string_view name);
/// Tells whether a text can be the text of a SERVER_MESSAGE: 1 to 200 bytes of UTF-8, without control
/// characters.
[[nodiscard]] bool isValidServerMessage(std::string_view text);
/// Tells whether a text can be the locale of HELLO: 2 to 16 of `A-Z a-z 0-9 - _`.
[[nodiscard]] bool isValidLocale(std::string_view locale);
/// Tells whether a text can be a code sent by email: 6 digits.
[[nodiscard]] bool isValidEmailCode(std::string_view code);
/// Tells whether a text can be the code verifier of PKCE: 43 to 128 of `A-Z a-z 0-9 - . _ ~` (RFC 7636).
[[nodiscard]] bool isValidCodeVerifier(std::string_view verifier);
/// Tells whether a text can be the scopes of DISCORD_AUTHORIZE: 1 to 255 of `A-Z a-z 0-9 . _`, separated by
/// single spaces.
[[nodiscard]] bool isValidDiscordScopes(std::string_view scopes);
/// Tells whether a text can be a token of Discord: 1 to 1024 printable ASCII characters, without space.
[[nodiscard]] bool isValidDiscordToken(std::string_view token);

// --- Client to master ---

/// `HELLO`: the first message of a client.
struct HelloMessage {
    /// Decoded as sent: the master answers UNSUPPORTED_VERSION if it is not ProtocolVersion. The version is
    /// the first field in every version: a HELLO of another version is decoded as its version alone, the rest
    /// left unread, since its layout is not this one.
    std::uint16_t version = ProtocolVersion;
    /// The name of a guest, decoded as sent: the master answers INVALID_NAME if !isValidPlayerName(). Empty
    /// with a token, which signs in an account: anything else is malformed.
    std::string name;
    /// The language of the player, for the emails of the master: isValidLocale(), or the HELLO is malformed.
    std::string locale = "en";
    /// The session token of an account, to resume its session.
    std::optional<SessionToken> token;

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

/// `REGISTER`: creates an account, whose email gets a code (version 3).
struct RegisterMessage {
    /// 1 to MaxUsernameLength bytes of text.
    std::string username;
    /// 1 to MaxEmailLength bytes of text.
    std::string email;
    /// 1 to MaxPasswordLength bytes of text. Secret.
    std::string password;

    friend bool operator==(const RegisterMessage &, const RegisterMessage &) = default;
};

/// `VERIFY_EMAIL`: the code that REGISTER, RESEND_CODE or SIGN_IN sent (version 3).
struct VerifyEmailMessage {
    /// The username or the email of the account: 1 to MaxEmailLength bytes of text.
    std::string login;
    /// isValidEmailCode(), or the message is malformed. Secret.
    std::string code;

    friend bool operator==(const VerifyEmailMessage &, const VerifyEmailMessage &) = default;
};

/// `RESEND_CODE`: a new code to verify the email (version 3).
struct ResendCodeMessage {
    /// The username or the email of the account: 1 to MaxEmailLength bytes of text.
    std::string login;

    friend bool operator==(const ResendCodeMessage &, const ResendCodeMessage &) = default;
};

/// `SIGN_IN` (version 3).
struct SignInMessage {
    /// The username or the email of the account: 1 to MaxEmailLength bytes of text.
    std::string login;
    /// 1 to MaxPasswordLength bytes of text. Secret.
    std::string password;

    friend bool operator==(const SignInMessage &, const SignInMessage &) = default;
};

/// `FORGOT_PASSWORD`: a code to choose a new password (version 3).
struct ForgotPasswordMessage {
    /// 1 to MaxEmailLength bytes of text.
    std::string email;

    friend bool operator==(const ForgotPasswordMessage &, const ForgotPasswordMessage &) = default;
};

/// `RESET_PASSWORD`: the code of FORGOT_PASSWORD and the new password (version 3).
struct ResetPasswordMessage {
    /// 1 to MaxEmailLength bytes of text.
    std::string email;
    /// isValidEmailCode(), or the message is malformed. Secret.
    std::string code;
    /// 1 to MaxPasswordLength bytes of text. Secret.
    std::string password;

    friend bool operator==(const ResetPasswordMessage &, const ResetPasswordMessage &) = default;
};

/// `DISCORD_CONFIRM`: what Discord gave the client once the player accepted (version 3), for the master to
/// exchange with Discord. The three fields go to Discord as they are: Discord checks them.
struct DiscordConfirmMessage {
    /// The authorization code: 1 to MaxDiscordCodeLength bytes of text. Secret.
    std::string code;
    /// The code verifier of PKCE whose challenge the client gave Discord: isValidCodeVerifier(), or the
    /// message is malformed. Secret.
    std::string codeVerifier;
    /// The address Discord sent the code back to, on the machine of the player: 1 to MaxRedirectUriLength
    /// bytes of text.
    std::string redirectUri;

    friend bool operator==(const DiscordConfirmMessage &, const DiscordConfirmMessage &) = default;
};

/// `CHOOSE_USERNAME`: the username of the account of a new Discord identity (version 3).
struct ChooseUsernameMessage {
    /// 1 to MaxUsernameLength bytes of text.
    std::string username;

    friend bool operator==(const ChooseUsernameMessage &, const ChooseUsernameMessage &) = default;
};

/// `DELETE_ACCOUNT`: deletes the account signed in on this connection (version 3).
struct DeleteAccountMessage {
    /// 0 to MaxPasswordLength bytes of text. Empty for an account without password, which confirms with
    /// Discord instead. Secret.
    std::string password;

    friend bool operator==(const DeleteAccountMessage &, const DeleteAccountMessage &) = default;
};

/// `GET_PROFILE`: the Discord profile of an account (version 4).
struct GetProfileMessage {
    /// Never 0, which is malformed.
    std::uint64_t accountId = 0;

    friend bool operator==(const GetProfileMessage &, const GetProfileMessage &) = default;
};

/// `GET_AVATAR`: a part of the picture of an account, from `offset` (version 4).
struct GetAvatarMessage {
    /// 0 for the new Discord user confirmed on this connection, who has no account yet (CHOOSE_USERNAME).
    std::uint64_t accountId = 0;
    std::uint32_t offset = 0;

    friend bool operator==(const GetAvatarMessage &, const GetAvatarMessage &) = default;
};

// --- Master to client ---

/// What a master offers, in WELCOME (version 3).
struct ServerFeatures {
    /// Players can play without an account.
    bool guests = true;
    /// Accounts with an email and a password: REGISTER, SIGN_IN and the others.
    bool passwordAccounts = false;
    /// Sign-in with Discord: DISCORD_SIGN_IN.
    bool discord = false;

    friend bool operator==(const ServerFeatures &, const ServerFeatures &) = default;
};

/// The role of an account.
enum class AccountRole : std::uint8_t {
    Player = 0,
    /// Reaches the administration panel.
    Admin = 1,
};

/// An account signed in, in WELCOME and SIGNED_IN (version 3), with what it knows of itself (version 4).
struct AccountInfo {
    /// Never 0.
    std::uint64_t id = 0;
    /// 1 to MaxUsernameLength bytes of text: the name of the player.
    std::string username;
    AccountRole role = AccountRole::Player;
    /// 0 to MaxEmailLength bytes of text: empty for an account made with Discord (version 4, as below).
    std::string email{};
    bool emailVerified = false;
    /// Whether it signs in with a password: an account made with Discord has none.
    bool hasPassword = false;
    /// When it was made, in milliseconds since 1970 (UTC).
    std::uint64_t createdAt = 0;

    friend bool operator==(const AccountInfo &, const AccountInfo &) = default;
};

/// `WELCOME`: the answer to HELLO.
struct WelcomeMessage {
    /// The id of the player on this master, never 0.
    std::uint32_t playerId = 0;
    /// What the master offers (version 3).
    ServerFeatures features;
    /// The account the token of HELLO signed in, if any (version 3).
    std::optional<AccountInfo> account;

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
    /// The secret of the Discord lobby of the game, the same for all its players (version 7): those with
    /// Discord join it for the voice. None when the master gave the player no Discord token. Secret: never
    /// logged.
    std::optional<std::array<std::byte, 32>> voiceSecret;

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
    /// later; the connection is closed after it (version 2). From version 3, the account is banned too: the
    /// answer to SIGN_IN after the right password, or sent without a request. A ban ends the sessions of the
    /// account: its token gets SESSION_EXPIRED.
    Banned = 14,
    /// The master does not offer this way of signing in (WELCOME tells what it offers) (version 3, as all the
    /// codes below).
    AccountsDisabled = 15,
    /// Another account has this username, ignoring case.
    UsernameTaken = 16,
    /// The password is not 10 to 128 characters, or is the username or the email.
    WeakPassword = 17,
    /// No account with this login and this password.
    InvalidCredentials = 18,
    /// The password was right, but the email is not verified: a new code was sent.
    EmailNotVerified = 19,
    InvalidCode = 20,
    CodeExpired = 21,
    /// Too many tries: of this code, of codes today, or of requests from this address.
    TooManyAttempts = 22,
    /// The token of HELLO is unknown or expired: the client forgets it and connects again without it. The
    /// connection is closed after the error.
    SessionExpired = 23,
    /// The master cannot check a password now: try again in a moment.
    Busy = 24,
    /// The master accepts no guest: CREATE_GAME and JOIN_GAME need an account signed in.
    GuestsDisabled = 25,
    /// The name of HELLO is the username of an account.
    NameReserved = 26,
    /// The master cannot send emails now.
    MailUnavailable = 27,
    /// No Discord confirmation on this connection, or older than five minutes.
    DiscordNotConfirmed = 28,
    /// Not an email address.
    InvalidEmail = 29,
    /// SIGN_OUT or DELETE_ACCOUNT without an account signed in.
    NotSignedIn = 30,
    /// A request to sign in, or to create an account, while signed in.
    AlreadySignedIn = 31,
    /// This Discord identity belongs to another account.
    DiscordTaken = 32,
    /// The master cannot reach Discord now, or Discord answers with an error: try again in a moment.
    DiscordUnavailable = 33,
    /// GET_PROFILE of no account, or GET_AVATAR of an account without picture, or from beyond its end
    /// (version 4).
    NotFound = 34,
    /// Discord refuses the refresh token of DISCORD_REFRESH, or it is not the one of the Discord identity of
    /// the account: the client forgets it (version 6).
    DiscordTokenRefused = 35,
};

/// Tells whether the master closes the connection after this error: MALFORMED, UNSUPPORTED_VERSION, KICKED,
/// BANNED and SESSION_EXPIRED.
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

/// `SIGNED_IN`: the connection is signed in, the answer to VERIFY_EMAIL, SIGN_IN, RESET_PASSWORD,
/// DISCORD_CONFIRM and CHOOSE_USERNAME (version 3).
struct SignedInMessage {
    /// To send in the next HELLO. Secret: never logged.
    SessionToken token{};
    AccountInfo account;

    friend bool operator==(const SignedInMessage &, const SignedInMessage &) = default;
};

/// `PROFILE`: the answer to GET_PROFILE, the Discord profile of an account as of its last sign-in with
/// Discord; and the answer to DISCORD_CONFIRM for a new Discord user, who chooses its username next (version
/// 4).
struct ProfileMessage {
    /// 0 in the answer to DISCORD_CONFIRM: the new Discord user, who has no account yet.
    std::uint64_t accountId = 0;
    /// Whether the account is linked to a Discord user. Without, the texts are empty and the size 0.
    bool discord = false;
    /// 0 to MaxDiscordUsernameLength bytes of text: `@username` on Discord.
    std::string discordUsername{};
    /// 0 to MaxDisplayNameLength bytes of text: the name it shows on Discord.
    std::string displayName{};
    /// 0 to MaxAvatarHashLength bytes of text: the version of its picture, empty for none. A client keeps a
    /// picture under it, and asks GET_AVATAR only for a version it does not have.
    std::string avatarHash{};
    /// The bytes of the picture, 0 for none, at most MaxAvatarSize.
    std::uint32_t avatarSize = 0;

    friend bool operator==(const ProfileMessage &, const ProfileMessage &) = default;
};

/// `AVATAR`: the answer to GET_AVATAR, a part of the picture of an account, a PNG (version 4). The client
/// asks the next part from `offset` plus the size of this one, until it has `total` bytes, and checks that
/// the version did not change meanwhile.
struct AvatarMessage {
    /// As in GET_AVATAR: 0 for the new Discord user of this connection.
    std::uint64_t accountId = 0;
    /// 1 to MaxAvatarHashLength bytes of text.
    std::string avatarHash;
    /// 1 to MaxAvatarSize.
    std::uint32_t total = 0;
    /// Where the part starts: less than `total`.
    std::uint32_t offset = 0;
    /// 1 to MaxAvatarChunk bytes, ending at `total` at most.
    std::vector<std::byte> bytes;

    friend bool operator==(const AvatarMessage &, const AvatarMessage &) = default;
};

/// `PROGRESS`: the progression of the account signed in, once signed in and after each game online that
/// counted for it. Guests never receive it.
struct ProgressMessage {
    /// The experience of the account, all its games: its level is rtype::game::levelOf(xp).
    std::uint64_t xp = 0;
    /// The experience the game that just ended gave, 0 when sent at the sign-in.
    std::uint32_t gained = 0;
    std::uint32_t gamesPlayed = 0;
    std::uint32_t totalKills = 0;
    /// The most waves cleared in a game, and the longest streak.
    std::uint16_t bestWaves = 0;
    std::uint16_t bestStreak = 0;

    friend bool operator==(const ProgressMessage &, const ProgressMessage &) = default;
};

/// `DISCORD_AUTHORIZE`: the answer to DISCORD_SIGN_IN, the Discord application of the master, for the client
/// to ask the player through the Discord Social SDK (version 3).
struct DiscordAuthorizeMessage {
    /// The id of the application (its client id): never 0, which is malformed.
    std::uint64_t applicationId = 0;
    /// What the master asks of the player (version 6): `identify`, or `identify openid sdk.social_layer` when
    /// the client gets a token of the player (`[discord] social`). isValidDiscordScopes().
    std::string scopes = "identify";

    friend bool operator==(const DiscordAuthorizeMessage &, const DiscordAuthorizeMessage &) = default;
};

/// `DISCORD_TOKEN`: the token of the player on Discord, for the Social SDK of its client (version 6). Sent
/// before the SIGNED_IN or DONE of a DISCORD_CONFIRM, and before the SIGNED_IN of the CHOOSE_USERNAME of a
/// new Discord user, when the master has the social features; and as the answer to DISCORD_REFRESH. The
/// master keeps neither token. Secret: never logged.
struct DiscordTokenMessage {
    /// isValidDiscordToken(), both.
    std::string accessToken;
    std::string refreshToken;
    /// The access token expires in this many seconds.
    std::uint32_t expiresIn = 0;

    friend bool operator==(const DiscordTokenMessage &, const DiscordTokenMessage &) = default;
};

/// `DISCORD_REFRESH`: a new access token from the refresh token of DISCORD_TOKEN, which only the master can
/// exchange, with the secret of its application (version 6). Signed in to an account linked to Discord only.
/// Secret: never logged.
struct DiscordRefreshMessage {
    /// isValidDiscordToken().
    std::string refreshToken;

    friend bool operator==(const DiscordRefreshMessage &, const DiscordRefreshMessage &) = default;
};

/// The payloads. LIST_GAMES, LEAVE_GAME, PING, SIGN_OUT, DISCORD_SIGN_IN, PONG, CODE_SENT and DONE have none:
/// send an empty payload.
/// @throws std::invalid_argument for a value the protocol forbids.
[[nodiscard]] std::vector<std::byte> encode(const HelloMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const CreateGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const JoinGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const RegisterMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VerifyEmailMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ResendCodeMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const SignInMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ForgotPasswordMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ResetPasswordMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const DiscordConfirmMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ChooseUsernameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const DeleteAccountMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GetProfileMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GetAvatarMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const WelcomeMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameListMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameJoinedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const LeftGameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ErrorMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ServerMessageMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const SignedInMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const DiscordAuthorizeMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ProfileMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const AvatarMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const ProgressMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const DiscordTokenMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const DiscordRefreshMessage &message);

[[nodiscard]] std::optional<HelloMessage> decodeHello(std::span<const std::byte> payload);
[[nodiscard]] std::optional<CreateGameMessage> decodeCreateGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<JoinGameMessage> decodeJoinGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<RegisterMessage> decodeRegister(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VerifyEmailMessage> decodeVerifyEmail(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ResendCodeMessage> decodeResendCode(std::span<const std::byte> payload);
[[nodiscard]] std::optional<SignInMessage> decodeSignIn(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ForgotPasswordMessage> decodeForgotPassword(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ResetPasswordMessage> decodeResetPassword(std::span<const std::byte> payload);
[[nodiscard]] std::optional<DiscordConfirmMessage> decodeDiscordConfirm(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ChooseUsernameMessage> decodeChooseUsername(std::span<const std::byte> payload);
[[nodiscard]] std::optional<DeleteAccountMessage> decodeDeleteAccount(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GetProfileMessage> decodeGetProfile(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GetAvatarMessage> decodeGetAvatar(std::span<const std::byte> payload);
[[nodiscard]] std::optional<WelcomeMessage> decodeWelcome(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameListMessage> decodeGameList(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameJoinedMessage> decodeGameJoined(std::span<const std::byte> payload);
[[nodiscard]] std::optional<LeftGameMessage> decodeLeftGame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ErrorMessage> decodeError(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ServerMessageMessage> decodeServerMessage(std::span<const std::byte> payload);
[[nodiscard]] std::optional<SignedInMessage> decodeSignedIn(std::span<const std::byte> payload);
[[nodiscard]] std::optional<DiscordAuthorizeMessage>
decodeDiscordAuthorize(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ProfileMessage> decodeProfile(std::span<const std::byte> payload);
[[nodiscard]] std::optional<AvatarMessage> decodeAvatar(std::span<const std::byte> payload);
[[nodiscard]] std::optional<ProgressMessage> decodeProgress(std::span<const std::byte> payload);
[[nodiscard]] std::optional<DiscordTokenMessage> decodeDiscordToken(std::span<const std::byte> payload);
[[nodiscard]] std::optional<DiscordRefreshMessage> decodeDiscordRefresh(std::span<const std::byte> payload);
/// For the messages without payload: true if it is empty.
[[nodiscard]] bool decodeEmpty(std::span<const std::byte> payload);

} // namespace rtype::lobby
