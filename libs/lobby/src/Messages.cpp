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

} // namespace

bool isValidPlayerName(std::string_view name)
{
    return !name.empty() && name.size() <= MaxPlayerNameLength && rtype::network::isValidText(name);
}

bool isValidGameName(std::string_view name)
{
    return !name.empty() && name.size() <= MaxGameNameLength && rtype::network::isValidText(name);
}

bool closesConnection(ErrorCode code)
{
    return code == ErrorCode::Malformed || code == ErrorCode::UnsupportedVersion;
}

// --- Client to master ---

std::vector<std::byte> encode(const HelloMessage &message)
{
    require(isValidPlayerName(message.name), "A player name is 1 to 16 bytes of text");
    Writer writer(3 + message.name.size());
    writer.writeU16(message.version);
    writer.writeString(message.name);
    return writer.take();
}

std::optional<HelloMessage> decodeHello(std::span<const std::byte> payload)
{
    Reader reader(payload);
    HelloMessage message;
    message.version = reader.readU16();
    message.name = readRawString(reader);
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

// --- Master to client ---

std::vector<std::byte> encode(const WelcomeMessage &message)
{
    require(message.playerId != 0, "A player id is never 0");
    Writer writer(4);
    writer.writeU32(message.playerId);
    return writer.take();
}

std::optional<WelcomeMessage> decodeWelcome(std::span<const std::byte> payload)
{
    Reader reader(payload);
    WelcomeMessage message;
    message.playerId = reader.readU32();
    if (message.playerId == 0) {
        reader.fail();
    }
    return finished(reader, message);
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
    if (reason > static_cast<std::uint8_t>(LeftGameReason::TimedOut)) {
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

bool decodeEmpty(std::span<const std::byte> payload)
{
    return payload.empty();
}

} // namespace rtype::lobby
