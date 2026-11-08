#include "Common/InstanceMessages.hpp"

#include "rtype/network/Reader.hpp"
#include "rtype/network/Writer.hpp"

#include <algorithm>
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

std::vector<std::byte> encode(const GameStatusMessage &message)
{
    require(message.players.size() <= MaxInstancePlayers, "A GAME_STATUS has at most 4 players");
    for (std::size_t i = 0; i < message.players.size(); ++i) {
        const PlayerStatus &player = message.players[i];
        require(!player.latency || *player.latency <= MaxStatusLatency, "A latency is at most 65534 ms");
        for (std::size_t j = 0; j < i; ++j) {
            require(message.players[j].playerId != player.playerId, "A player is in a GAME_STATUS once");
        }
    }
    Writer writer(7 + (11 * message.players.size()));
    writer.writeU32(message.tick);
    writer.writeU16(message.wave);
    writer.writeU8(static_cast<std::uint8_t>(message.players.size()));
    for (const PlayerStatus &player : message.players) {
        writer.writeU32(player.playerId);
        writer.writeU32(player.score);
        writer.writeU8(player.alive ? 1 : 0);
        writer.writeU16(player.latency.value_or(0xFFFF));
    }
    return writer.take();
}

std::optional<GameStatusMessage> decodeGameStatus(std::span<const std::byte> payload)
{
    Reader reader(payload);
    GameStatusMessage message;
    message.tick = reader.readU32();
    message.wave = reader.readU16();
    const std::uint8_t count = reader.readU8();
    if (count > MaxInstancePlayers) {
        reader.fail();
    }
    for (std::uint8_t i = 0; i < count && reader.ok(); ++i) {
        PlayerStatus player;
        player.playerId = reader.readU32();
        player.score = reader.readU32();
        const std::uint8_t alive = reader.readU8();
        const std::uint16_t latency = reader.readU16();
        if (alive > 1 || std::ranges::any_of(message.players, [&](const PlayerStatus &other) {
                return other.playerId == player.playerId;
            })) {
            reader.fail();
        }
        player.alive = alive == 1;
        if (latency != 0xFFFF) {
            player.latency = latency;
        }
        message.players.push_back(player);
    }
    return finished(reader, message);
}

// --- Master to instance ---

std::vector<std::byte> encode(const ConfigureMessage &message)
{
    require(!message.name.empty() && message.name.size() <= MaxGameNameLength,
            "A game name is 1 to 32 bytes");
    require(message.maxPlayers >= 1 && message.maxPlayers <= MaxInstancePlayers, "A game has 1 to 4 players");
    Writer writer(4 + message.name.size());
    writer.writeString(message.name);
    writer.writeU8(message.maxPlayers);
    writer.writeU16(message.relayPort);
    return writer.take();
}

std::optional<ConfigureMessage> decodeConfigure(std::span<const std::byte> payload)
{
    Reader reader(payload);
    ConfigureMessage message;
    message.name = reader.readString(1, MaxGameNameLength);
    message.maxPlayers = reader.readU8();
    message.relayPort = reader.readU16();
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
    Writer writer(81 + message.name.size());
    writer.writeU32(message.playerId);
    writer.writeU32(message.sessionId);
    writer.writeBytes(message.clientKey);
    writer.writeBytes(message.serverKey);
    writer.writeString(message.name);
    writer.writeU64(message.accountId);
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
    message.accountId = reader.readU64();
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
    if (reason > static_cast<std::uint8_t>(RemovePlayerReason::Kicked)) {
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
    if (reason > static_cast<std::uint8_t>(ShutdownReason::EndedByAdmin)) {
        reader.fail();
    }
    return finished(reader, ShutdownMessage{static_cast<ShutdownReason>(reason)});
}

} // namespace rtype::server
