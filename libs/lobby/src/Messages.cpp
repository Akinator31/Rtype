#include "rtype/lobby/Messages.hpp"

#include "rtype/network/Reader.hpp"
#include "rtype/network/Text.hpp"
#include "rtype/network/Writer.hpp"

#include <stdexcept>

namespace rtype::lobby
{

namespace
{

using rtype::network::Reader;
using rtype::network::Writer;

void require(bool condition, const char *problem)
{
    if (!condition) {
        throw std::invalid_argument(problem);
    }
}

/// A str8 read as it comes, without checking its text: the master answers INVALID_NAME for a bad one.
std::string readRawString(Reader &reader)
{
    const std::uint8_t length = reader.readU8();
    const std::span<const std::byte> bytes = reader.readBytes(length);
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

template <typename Message>
std::optional<Message> finished(Reader &reader, Message message)
{
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

/// A text of a field of the accounts, whose length is checked by the encoder: the reader checks it again.
void writeText(Writer &writer, std::string_view text, std::size_t maxLength, const char *problem)
{
    require(!text.empty() && text.size() <= maxLength && rtype::network::isValidText(text), problem);
    writer.writeString(text);
}

void writePassword(Writer &writer, std::string_view password, bool allowEmpty)
{
    require((allowEmpty || !password.empty()) && password.size() <= MaxPasswordLength &&
                rtype::network::isValidText(password),
            "A password is 1 to 512 bytes of text");
    writer.writeString16(password);
}

std::string readPassword(Reader &reader, bool allowEmpty)
{
    return reader.readString16(allowEmpty ? 0 : 1, MaxPasswordLength);
}

std::string readEmailCode(Reader &reader)
{
    std::string code = reader.readString(CodeLength, CodeLength);
    if (reader.ok() && !isValidEmailCode(code)) {
        reader.fail();
    }
    return code;
}

/// A text that may be empty, of at most `maxLength` bytes.
void writeOptionalText(Writer &writer, std::string_view text, std::size_t maxLength, const char *problem)
{
    require(text.size() <= maxLength && rtype::network::isValidText(text), problem);
    writer.writeString(text);
}

namespace account_bit
{
constexpr std::uint8_t EmailVerified = 0x01;
constexpr std::uint8_t HasPassword = 0x02;
constexpr std::uint8_t All = EmailVerified | HasPassword;
} // namespace account_bit

void writeAccount(Writer &writer, const AccountInfo &account)
{
    require(account.id != 0, "An account id is never 0");
    require(account.role == AccountRole::Player || account.role == AccountRole::Admin, "Unknown role");
    writer.writeU64(account.id);
    writeText(writer, account.username, MaxUsernameLength, "A username is 1 to 16 bytes of text");
    writer.writeU8(static_cast<std::uint8_t>(account.role));
    writer.writeU8(static_cast<std::uint8_t>((account.emailVerified ? account_bit::EmailVerified : 0) |
                                             (account.hasPassword ? account_bit::HasPassword : 0)));
    writeOptionalText(writer, account.email, MaxEmailLength, "An email is 0 to 254 bytes of text");
    writer.writeU64(account.createdAt);
}

AccountInfo readAccount(Reader &reader)
{
    AccountInfo account;
    account.id = reader.readU64();
    account.username = reader.readString(1, MaxUsernameLength);
    const std::uint8_t role = reader.readU8();
    const std::uint8_t flags = reader.readU8();
    account.email = reader.readString(0, MaxEmailLength);
    account.createdAt = reader.readU64();
    if (account.id == 0 || role > static_cast<std::uint8_t>(AccountRole::Admin) ||
        (flags & ~account_bit::All) != 0) {
        reader.fail();
    }
    account.role = static_cast<AccountRole>(role);
    account.emailVerified = (flags & account_bit::EmailVerified) != 0;
    account.hasPassword = (flags & account_bit::HasPassword) != 0;
    return account;
}

namespace feature_bit
{
constexpr std::uint8_t Guests = 0x01;
constexpr std::uint8_t PasswordAccounts = 0x02;
constexpr std::uint8_t Discord = 0x04;
constexpr std::uint8_t All = Guests | PasswordAccounts | Discord;
} // namespace feature_bit

} // namespace

bool isAccountRequest(std::uint8_t type)
{
    return type >= message_type::Register && type <= message_type::GetAvatar;
}

bool isValidPlayerName(std::string_view name)
{
    return !name.empty() && name.size() <= MaxPlayerNameLength && rtype::network::isValidText(name);
}

bool isValidGameName(std::string_view name)
{
    return !name.empty() && name.size() <= MaxGameNameLength && rtype::network::isValidText(name);
}

bool isValidServerMessage(std::string_view text)
{
    return !text.empty() && text.size() <= MaxServerMessageLength && rtype::network::isValidText(text);
}

bool isValidLocale(std::string_view locale)
{
    if (locale.size() < MinLocaleLength || locale.size() > MaxLocaleLength) {
        return false;
    }
    for (const char c : locale) {
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        if (!letter && !digit && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

bool isValidEmailCode(std::string_view code)
{
    if (code.size() != CodeLength) {
        return false;
    }
    for (const char c : code) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool isValidCodeVerifier(std::string_view verifier)
{
    if (verifier.size() < MinCodeVerifierLength || verifier.size() > MaxCodeVerifierLength) {
        return false;
    }
    for (const char c : verifier) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                c == '-' || c == '.' || c == '_' || c == '~';
        if (!unreserved) {
            return false;
        }
    }
    return true;
}

bool closesConnection(ErrorCode code)
{
    return code == ErrorCode::Malformed || code == ErrorCode::UnsupportedVersion ||
           code == ErrorCode::Kicked || code == ErrorCode::Banned || code == ErrorCode::SessionExpired;
}

// --- Client to master ---

std::vector<std::byte> encode(const HelloMessage &message)
{
    if (message.token) {
        require(message.name.empty(), "A HELLO with a session token has no name");
    } else {
        require(isValidPlayerName(message.name), "A player name is 1 to 16 bytes of text");
    }
    require(isValidLocale(message.locale), "A locale is 2 to 16 of A-Z a-z 0-9 - _");
    Writer writer(6 + message.name.size() + message.locale.size() + (message.token ? 32 : 0));
    writer.writeU16(message.version);
    writer.writeString(message.name);
    writer.writeString(message.locale);
    writer.writeBool(message.token.has_value());
    if (message.token) {
        writer.writeBytes(*message.token);
    }
    return writer.take();
}

std::optional<HelloMessage> decodeHello(std::span<const std::byte> payload)
{
    Reader reader(payload);
    HelloMessage message;
    message.version = reader.readU16();
    if (reader.ok() && message.version != ProtocolVersion) {
        // Another layout follows: the master answers UNSUPPORTED_VERSION from the version alone
        message.locale.clear();
        return message;
    }
    message.name = readRawString(reader);
    message.locale = reader.readString(MinLocaleLength, MaxLocaleLength);
    if (reader.ok() && !isValidLocale(message.locale)) {
        reader.fail();
    }
    if (reader.readBool()) {
        message.token = reader.readBytes<32>();
        if (!message.name.empty()) {
            reader.fail();
        }
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const CreateGameMessage &message)
{
    require(isValidGameName(message.name), "A game name is 1 to 32 bytes of text");
    require(message.maxPlayers >= 1 && message.maxPlayers <= MaxPlayers, "A game has 1 to 4 players");
    Writer writer(2 + message.name.size());
    writer.writeString(message.name);
    writer.writeU8(message.maxPlayers);
    return writer.take();
}

std::optional<CreateGameMessage> decodeCreateGame(std::span<const std::byte> payload)
{
    Reader reader(payload);
    CreateGameMessage message;
    message.name = readRawString(reader);
    message.maxPlayers = reader.readU8();
    if (message.maxPlayers == 0 || message.maxPlayers > MaxPlayers) {
        reader.fail();
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const JoinGameMessage &message)
{
    Writer writer(4);
    writer.writeU32(message.gameId);
    return writer.take();
}

std::optional<JoinGameMessage> decodeJoinGame(std::span<const std::byte> payload)
{
    Reader reader(payload);
    JoinGameMessage message;
    message.gameId = reader.readU32();
    return finished(reader, message);
}

std::vector<std::byte> encode(const RegisterMessage &message)
{
    Writer writer;
    writeText(writer, message.username, MaxUsernameLength, "A username is 1 to 16 bytes of text");
    writeText(writer, message.email, MaxEmailLength, "An email is 1 to 254 bytes of text");
    writePassword(writer, message.password, false);
    return writer.take();
}

std::optional<RegisterMessage> decodeRegister(std::span<const std::byte> payload)
{
    Reader reader(payload);
    RegisterMessage message;
    message.username = reader.readString(1, MaxUsernameLength);
    message.email = reader.readString(1, MaxEmailLength);
    message.password = readPassword(reader, false);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const VerifyEmailMessage &message)
{
    require(isValidEmailCode(message.code), "A code sent by email is 6 digits");
    Writer writer;
    writeText(writer, message.login, MaxEmailLength, "A login is 1 to 254 bytes of text");
    writer.writeString(message.code);
    return writer.take();
}

std::optional<VerifyEmailMessage> decodeVerifyEmail(std::span<const std::byte> payload)
{
    Reader reader(payload);
    VerifyEmailMessage message;
    message.login = reader.readString(1, MaxEmailLength);
    message.code = readEmailCode(reader);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const ResendCodeMessage &message)
{
    Writer writer;
    writeText(writer, message.login, MaxEmailLength, "A login is 1 to 254 bytes of text");
    return writer.take();
}

std::optional<ResendCodeMessage> decodeResendCode(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ResendCodeMessage message;
    message.login = reader.readString(1, MaxEmailLength);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const SignInMessage &message)
{
    Writer writer;
    writeText(writer, message.login, MaxEmailLength, "A login is 1 to 254 bytes of text");
    writePassword(writer, message.password, false);
    return writer.take();
}

std::optional<SignInMessage> decodeSignIn(std::span<const std::byte> payload)
{
    Reader reader(payload);
    SignInMessage message;
    message.login = reader.readString(1, MaxEmailLength);
    message.password = readPassword(reader, false);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const ForgotPasswordMessage &message)
{
    Writer writer;
    writeText(writer, message.email, MaxEmailLength, "An email is 1 to 254 bytes of text");
    return writer.take();
}

std::optional<ForgotPasswordMessage> decodeForgotPassword(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ForgotPasswordMessage message;
    message.email = reader.readString(1, MaxEmailLength);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const ResetPasswordMessage &message)
{
    require(isValidEmailCode(message.code), "A code sent by email is 6 digits");
    Writer writer;
    writeText(writer, message.email, MaxEmailLength, "An email is 1 to 254 bytes of text");
    writer.writeString(message.code);
    writePassword(writer, message.password, false);
    return writer.take();
}

std::optional<ResetPasswordMessage> decodeResetPassword(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ResetPasswordMessage message;
    message.email = reader.readString(1, MaxEmailLength);
    message.code = readEmailCode(reader);
    message.password = readPassword(reader, false);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const DiscordConfirmMessage &message)
{
    require(isValidCodeVerifier(message.codeVerifier), "A code verifier is 43 to 128 of A-Z a-z 0-9 - . _ ~");
    Writer writer;
    writeText(writer, message.code, MaxDiscordCodeLength, "A Discord code is 1 to 255 bytes of text");
    writer.writeString(message.codeVerifier);
    require(!message.redirectUri.empty() && message.redirectUri.size() <= MaxRedirectUriLength &&
                rtype::network::isValidText(message.redirectUri),
            "A redirect address is 1 to 2048 bytes of text");
    writer.writeString16(message.redirectUri);
    return writer.take();
}

std::optional<DiscordConfirmMessage> decodeDiscordConfirm(std::span<const std::byte> payload)
{
    Reader reader(payload);
    DiscordConfirmMessage message;
    message.code = reader.readString(1, MaxDiscordCodeLength);
    message.codeVerifier = reader.readString(MinCodeVerifierLength, MaxCodeVerifierLength);
    if (reader.ok() && !isValidCodeVerifier(message.codeVerifier)) {
        reader.fail();
    }
    message.redirectUri = reader.readString16(1, MaxRedirectUriLength);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const ChooseUsernameMessage &message)
{
    Writer writer;
    writeText(writer, message.username, MaxUsernameLength, "A username is 1 to 16 bytes of text");
    return writer.take();
}

std::optional<ChooseUsernameMessage> decodeChooseUsername(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ChooseUsernameMessage message;
    message.username = reader.readString(1, MaxUsernameLength);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const DeleteAccountMessage &message)
{
    Writer writer;
    writePassword(writer, message.password, true);
    return writer.take();
}

std::optional<DeleteAccountMessage> decodeDeleteAccount(std::span<const std::byte> payload)
{
    Reader reader(payload);
    DeleteAccountMessage message;
    message.password = readPassword(reader, true);
    return finished(reader, std::move(message));
}

// --- Master to client ---

std::vector<std::byte> encode(const WelcomeMessage &message)
{
    require(message.playerId != 0, "A player id is never 0");
    Writer writer;
    writer.writeU32(message.playerId);
    std::uint8_t features = 0;
    features |= message.features.guests ? feature_bit::Guests : 0;
    features |= message.features.passwordAccounts ? feature_bit::PasswordAccounts : 0;
    features |= message.features.discord ? feature_bit::Discord : 0;
    writer.writeU8(features);
    writer.writeBool(message.account.has_value());
    if (message.account) {
        writeAccount(writer, *message.account);
    }
    return writer.take();
}

std::optional<WelcomeMessage> decodeWelcome(std::span<const std::byte> payload)
{
    Reader reader(payload);
    WelcomeMessage message;
    message.playerId = reader.readU32();
    const std::uint8_t features = reader.readU8();
    if (message.playerId == 0 || (features & ~feature_bit::All) != 0) {
        reader.fail();
    }
    message.features.guests = (features & feature_bit::Guests) != 0;
    message.features.passwordAccounts = (features & feature_bit::PasswordAccounts) != 0;
    message.features.discord = (features & feature_bit::Discord) != 0;
    if (reader.readBool()) {
        message.account = readAccount(reader);
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const GameListMessage &message)
{
    require(message.games.size() <= MaxGamesInList, "A GAME_LIST has at most 256 games");
    Writer writer;
    writer.writeU16(static_cast<std::uint16_t>(message.games.size()));
    std::uint32_t previous = 0;
    for (const GameInfo &game : message.games) {
        require(game.id > previous, "The games of a GAME_LIST are sorted by id, each id once, never 0");
        require(isValidGameName(game.name), "A game name is 1 to 32 bytes of text");
        require(game.maxPlayers >= 1 && game.maxPlayers <= MaxPlayers, "A game has 1 to 4 players");
        previous = game.id;
        writer.writeU32(game.id);
        writer.writeString(game.name);
        writer.writeU8(game.players);
        writer.writeU8(game.maxPlayers);
    }
    return writer.take();
}

std::optional<GameListMessage> decodeGameList(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GameListMessage message;
    const std::uint16_t count = reader.readU16();
    if (count > MaxGamesInList) {
        reader.fail();
    }
    std::uint32_t previous = 0;
    for (std::uint16_t i = 0; i < count && reader.ok(); ++i) {
        GameInfo game;
        game.id = reader.readU32();
        game.name = reader.readString(1, MaxGameNameLength);
        game.players = reader.readU8();
        game.maxPlayers = reader.readU8();
        if (game.id <= previous || game.maxPlayers == 0 || game.maxPlayers > MaxPlayers) {
            reader.fail();
        }
        previous = game.id;
        message.games.push_back(std::move(game));
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const GameJoinedMessage &message)
{
    require(message.sessionId != 0, "A session id is never 0");
    Writer writer(74);
    writer.writeU32(message.gameId);
    writer.writeU16(message.udpPort);
    writer.writeU32(message.sessionId);
    writer.writeBytes(message.clientKey);
    writer.writeBytes(message.serverKey);
    return writer.take();
}

std::optional<GameJoinedMessage> decodeGameJoined(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GameJoinedMessage message;
    message.gameId = reader.readU32();
    message.udpPort = reader.readU16();
    message.sessionId = reader.readU32();
    message.clientKey = reader.readBytes<32>();
    message.serverKey = reader.readBytes<32>();
    if (message.sessionId == 0) {
        reader.fail();
    }
    return finished(reader, message);
}

std::vector<std::byte> encode(const LeftGameMessage &message)
{
    Writer writer(5);
    writer.writeU32(message.gameId);
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.take();
}

std::optional<LeftGameMessage> decodeLeftGame(std::span<const std::byte> payload)
{
    Reader reader(payload);
    LeftGameMessage message;
    message.gameId = reader.readU32();
    const std::uint8_t reason = reader.readU8();
    if (reason > static_cast<std::uint8_t>(LeftGameReason::EndedByAdmin)) {
        reader.fail();
    }
    message.reason = static_cast<LeftGameReason>(reason);
    return finished(reader, message);
}

std::vector<std::byte> encode(const ErrorMessage &message)
{
    Writer writer(3);
    writer.writeU8(message.request);
    writer.writeU16(static_cast<std::uint16_t>(message.code));
    return writer.take();
}

std::optional<ErrorMessage> decodeError(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ErrorMessage message;
    message.request = reader.readU8();
    message.code = static_cast<ErrorCode>(reader.readU16());
    return finished(reader, message);
}

std::vector<std::byte> encode(const ServerMessageMessage &message)
{
    require(isValidServerMessage(message.text), "A server message is 1 to 200 bytes of text");
    Writer writer(1 + message.text.size());
    writer.writeString(message.text);
    return writer.take();
}

std::optional<ServerMessageMessage> decodeServerMessage(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ServerMessageMessage message;
    message.text = reader.readString(1, MaxServerMessageLength);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const SignedInMessage &message)
{
    Writer writer;
    writer.writeBytes(message.token);
    writeAccount(writer, message.account);
    return writer.take();
}

std::optional<SignedInMessage> decodeSignedIn(std::span<const std::byte> payload)
{
    Reader reader(payload);
    SignedInMessage message;
    message.token = reader.readBytes<32>();
    message.account = readAccount(reader);
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const DiscordAuthorizeMessage &message)
{
    require(message.applicationId != 0, "A Discord application id is never 0");
    Writer writer(8);
    writer.writeU64(message.applicationId);
    return writer.take();
}

std::optional<DiscordAuthorizeMessage> decodeDiscordAuthorize(std::span<const std::byte> payload)
{
    Reader reader(payload);
    DiscordAuthorizeMessage message;
    message.applicationId = reader.readU64();
    if (reader.ok() && message.applicationId == 0) {
        reader.fail();
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const GetProfileMessage &message)
{
    require(message.accountId != 0, "An account id is never 0");
    Writer writer(8);
    writer.writeU64(message.accountId);
    return writer.take();
}

std::optional<GetProfileMessage> decodeGetProfile(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GetProfileMessage message;
    message.accountId = reader.readU64();
    if (reader.ok() && message.accountId == 0) {
        reader.fail();
    }
    return finished(reader, message);
}

std::vector<std::byte> encode(const GetAvatarMessage &message)
{
    Writer writer(12);
    writer.writeU64(message.accountId);
    writer.writeU32(message.offset);
    return writer.take();
}

std::optional<GetAvatarMessage> decodeGetAvatar(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GetAvatarMessage message;
    message.accountId = reader.readU64();
    message.offset = reader.readU32();
    return finished(reader, message);
}

std::vector<std::byte> encode(const ProfileMessage &message)
{
    require(message.discord || (message.discordUsername.empty() && message.displayName.empty() &&
                                message.avatarHash.empty() && message.avatarSize == 0),
            "A profile without Discord has no Discord name nor picture");
    require(message.avatarSize <= MaxAvatarSize, "A picture is at most 256 KiB");
    require(message.avatarHash.empty() == (message.avatarSize == 0), "A picture has a version, and a size");
    Writer writer;
    writer.writeU64(message.accountId);
    writer.writeBool(message.discord);
    writeOptionalText(writer, message.discordUsername, MaxDiscordUsernameLength,
                      "A Discord username is 0 to 64 bytes of text");
    writeOptionalText(writer, message.displayName, MaxDisplayNameLength,
                      "A display name is 0 to 128 bytes of text");
    writeOptionalText(writer, message.avatarHash, MaxAvatarHashLength,
                      "The version of a picture is 0 to 64 bytes of text");
    writer.writeU32(message.avatarSize);
    return writer.take();
}

std::optional<ProfileMessage> decodeProfile(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ProfileMessage message;
    message.accountId = reader.readU64();
    message.discord = reader.readBool();
    message.discordUsername = reader.readString(0, MaxDiscordUsernameLength);
    message.displayName = reader.readString(0, MaxDisplayNameLength);
    message.avatarHash = reader.readString(0, MaxAvatarHashLength);
    message.avatarSize = reader.readU32();
    if (reader.ok() &&
        (message.avatarSize > MaxAvatarSize || message.avatarHash.empty() != (message.avatarSize == 0) ||
         (!message.discord &&
          (!message.discordUsername.empty() || !message.displayName.empty() || message.avatarSize != 0)))) {
        reader.fail();
    }
    return finished(reader, std::move(message));
}

std::vector<std::byte> encode(const AvatarMessage &message)
{
    require(message.total >= 1 && message.total <= MaxAvatarSize, "A picture is 1 byte to 256 KiB");
    require(!message.bytes.empty() && message.bytes.size() <= MaxAvatarChunk &&
                message.offset < message.total && message.bytes.size() <= message.total - message.offset,
            "A part of a picture is 1 to 16000 bytes, within the picture");
    Writer writer(8 + 1 + message.avatarHash.size() + 4 + 4 + 2 + message.bytes.size());
    writer.writeU64(message.accountId);
    writeText(writer, message.avatarHash, MaxAvatarHashLength,
              "The version of a picture is 1 to 64 bytes of text");
    writer.writeU32(message.total);
    writer.writeU32(message.offset);
    writer.writeU16(static_cast<std::uint16_t>(message.bytes.size()));
    writer.writeBytes(message.bytes);
    return writer.take();
}

std::optional<AvatarMessage> decodeAvatar(std::span<const std::byte> payload)
{
    Reader reader(payload);
    AvatarMessage message;
    message.accountId = reader.readU64();
    message.avatarHash = reader.readString(1, MaxAvatarHashLength);
    message.total = reader.readU32();
    message.offset = reader.readU32();
    const std::uint16_t size = reader.readU16();
    const std::span<const std::byte> bytes = reader.readBytes(size);
    message.bytes.assign(bytes.begin(), bytes.end());
    if (reader.ok() &&
        (message.total == 0 || message.total > MaxAvatarSize || size == 0 || size > MaxAvatarChunk ||
         message.offset >= message.total || size > message.total - message.offset)) {
        reader.fail();
    }
    return finished(reader, std::move(message));
}

bool decodeEmpty(std::span<const std::byte> payload)
{
    return payload.empty();
}

} // namespace rtype::lobby
