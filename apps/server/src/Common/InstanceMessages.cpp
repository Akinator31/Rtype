#include "Common/InstanceMessages.hpp"

#include "rtype/network/Reader.hpp"
#include "rtype/network/Writer.hpp"

#include <stdexcept>

namespace rtype::server
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

/// The message, if the decoder accepted every value and no byte is left.
template <typename Message>
std::optional<Message> finished(Reader &reader, Message message)
{
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

} // namespace

// --- Instance to master ---

std::vector<std::byte> encode(const InstanceHelloMessage &message)
{
    require(message.id != 0, "An instance id is never 0");
    Writer writer(6 + SecretSize);
    writer.writeU16(message.version);
    writer.writeU32(message.id);
    writer.writeBytes(message.secret);
    return writer.take();
}

std::optional<InstanceHelloMessage> decodeInstanceHello(std::span<const std::byte> payload)
{
    Reader reader(payload);
    InstanceHelloMessage message;
    message.version = reader.readU16();
    message.id = reader.readU32();
    message.secret = reader.readBytes<SecretSize>();
    if (message.id == 0) {
        reader.fail();
    }
    return finished(reader, message);
}

std::vector<std::byte> encode(const ReadyMessage &message)
{
    Writer writer(2);
    writer.writeU16(message.udpPort);
    return writer.take();
}

std::optional<ReadyMessage> decodeReady(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ReadyMessage message;
    message.udpPort = reader.readU16();
    return finished(reader, message);
}

std::vector<std::byte> encode(const PlayerConnectedMessage &message)
{
    Writer writer(4);
    writer.writeU32(message.playerId);
    return writer.take();
}

std::optional<PlayerConnectedMessage> decodePlayerConnected(std::span<const std::byte> payload)
{
    Reader reader(payload);
    PlayerConnectedMessage message;
    message.playerId = reader.readU32();
    return finished(reader, message);
}

std::vector<std::byte> encode(const PlayerDisconnectedMessage &message)
{
    Writer writer(5);
    writer.writeU32(message.playerId);
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.take();
}

std::optional<PlayerDisconnectedMessage> decodePlayerDisconnected(std::span<const std::byte> payload)
{
    Reader reader(payload);
    PlayerDisconnectedMessage message;
    message.playerId = reader.readU32();
    const std::uint8_t reason = reader.readU8();
    if (reason > static_cast<std::uint8_t>(PlayerDisconnectedReason::TimedOut)) {
        reader.fail();
    }
    message.reason = static_cast<PlayerDisconnectedReason>(reason);
    return finished(reader, message);
}

std::vector<std::byte> encode(const GameOverReport &message)
{
    require(message.scores.size() <= MaxInstancePlayers, "A GAME_OVER has at most 4 scores");
    Writer writer(1 + (8 * message.scores.size()));
    writer.writeU8(static_cast<std::uint8_t>(message.scores.size()));
    for (const PlayerScore &score : message.scores) {
        writer.writeU32(score.playerId);
        writer.writeU32(score.score);
    }
    return writer.take();
}

std::optional<GameOverReport> decodeGameOverReport(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GameOverReport message;
    const std::uint8_t count = reader.readU8();
    if (count > MaxInstancePlayers) {
        reader.fail();
    }
    for (std::uint8_t i = 0; i < count && reader.ok(); ++i) {
        PlayerScore score;
        score.playerId = reader.readU32();
        score.score = reader.readU32();
        message.scores.push_back(score);
    }
    return finished(reader, message);
}

// --- Master to instance ---

std::vector<std::byte> encode(const ConfigureMessage &message)
{
    require(!message.name.empty() && message.name.size() <= MaxGameNameLength,
            "A game name is 1 to 32 bytes");
    require(message.maxPlayers >= 1 && message.maxPlayers <= MaxInstancePlayers, "A game has 1 to 4 players");
    Writer writer(2 + message.name.size());
    writer.writeString(message.name);
    writer.writeU8(message.maxPlayers);
    return writer.take();
}

std::optional<ConfigureMessage> decodeConfigure(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ConfigureMessage message;
    message.name = reader.readString(1, MaxGameNameLength);
    message.maxPlayers = reader.readU8();
    if (message.maxPlayers == 0 || message.maxPlayers > MaxInstancePlayers) {
        reader.fail();
    }
    return finished(reader, message);
}

std::vector<std::byte> encode(const AddPlayerMessage &message)
{
    require(message.sessionId != 0, "A session id is never 0");
    require(!message.name.empty() && message.name.size() <= MaxInstancePlayerNameLength,
            "A player name is 1 to 16 bytes");
    Writer writer(73 + message.name.size());
    writer.writeU32(message.playerId);
    writer.writeU32(message.sessionId);
    writer.writeBytes(message.clientKey);
    writer.writeBytes(message.serverKey);
    writer.writeString(message.name);
    return writer.take();
}

std::optional<AddPlayerMessage> decodeAddPlayer(std::span<const std::byte> payload)
{
    Reader reader(payload);
    AddPlayerMessage message;
    message.playerId = reader.readU32();
    message.sessionId = reader.readU32();
    message.clientKey = reader.readBytes<32>();
    message.serverKey = reader.readBytes<32>();
    message.name = reader.readString(1, MaxInstancePlayerNameLength);
    if (message.sessionId == 0) {
        reader.fail();
    }
    return finished(reader, message);
}

std::vector<std::byte> encode(const RemovePlayerMessage &message)
{
    Writer writer(5);
    writer.writeU32(message.playerId);
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.take();
}

std::optional<RemovePlayerMessage> decodeRemovePlayer(std::span<const std::byte> payload)
{
    Reader reader(payload);
    RemovePlayerMessage message;
    message.playerId = reader.readU32();
    const std::uint8_t reason = reader.readU8();
    if (reason > static_cast<std::uint8_t>(RemovePlayerReason::LobbyClosed)) {
        reader.fail();
    }
    message.reason = static_cast<RemovePlayerReason>(reason);
    return finished(reader, message);
}

std::vector<std::byte> encode(const ShutdownMessage &message)
{
    Writer writer(1);
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.take();
}

std::optional<ShutdownMessage> decodeShutdown(std::span<const std::byte> payload)
{
    Reader reader(payload);
    const std::uint8_t reason = reader.readU8();
    if (reason > static_cast<std::uint8_t>(ShutdownReason::MasterStopping)) {
        reader.fail();
    }
    return finished(reader, ShutdownMessage{static_cast<ShutdownReason>(reason)});
}

} // namespace rtype::server
