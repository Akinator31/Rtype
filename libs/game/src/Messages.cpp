#include "rtype/game/Messages.hpp"

#include "rtype/network/DatagramBody.hpp"
#include "rtype/network/Reader.hpp"
#include "rtype/network/Writer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rtype::game
{

namespace
{

using rtype::network::Reader;
using rtype::network::Writer;

/// The fields before the players in SNAPSHOT: tick, last input, part, parts, player count.
constexpr std::size_t SnapshotHeaderSize = 11;
/// One player in SNAPSHOT: slot, lives, score.
constexpr std::size_t PlayerStateSize = 6;
/// One entity in SNAPSHOT.
constexpr std::size_t EntityStateSize = 16;
/// The fields before the entities in ENTITIES_DESTROYED: tick, count.
constexpr std::size_t DestroyedHeaderSize = 5;
/// One entity in ENTITIES_DESTROYED: id, cause.
constexpr std::size_t DestroyedEntitySize = 5;
/// The bits of the flags of an entity that the protocol defines: the others are reserved.
constexpr std::uint8_t KnownFlags = EntityState::Invulnerable | EntityState::Charging | EntityState::Hit |
                                    EntityState::AimingUp | EntityState::AimingDown | EntityState::Guarding |
                                    EntityState::Slashing | EntityState::Shooting;
/// Eighths of a unit: the positions and velocities of the protocol (docs/protocol/game.md, section 9.1).
constexpr float FixedScale = 8.0f;

static_assert((rtype::network::MaxReliablePayloadSize - DestroyedHeaderSize) / DestroyedEntitySize ==
                  MaxDestroyedPerMessage,
              "MaxDestroyedPerMessage must follow the size of a datagram");

void require(bool condition, const char *problem)
{
    if (!condition) {
        throw std::invalid_argument(problem);
    }
}

bool isSlot(std::uint8_t slot)
{
    return slot < MaxPlayers;
}

/// A position or a velocity in eighths of a unit, rounded, and kept within an i16: a NaN is 0.
std::int16_t toFixed(float value)
{
    constexpr float lowest = std::numeric_limits<std::int16_t>::min();
    constexpr float highest = std::numeric_limits<std::int16_t>::max();
    if (std::isnan(value)) {
        return 0;
    }
    return static_cast<std::int16_t>(std::clamp(std::round(value * FixedScale), lowest, highest));
}

float fromFixed(std::int16_t value)
{
    return static_cast<float>(value) / FixedScale;
}

DestroyCause toCause(std::uint8_t value)
{
    return value == static_cast<std::uint8_t>(DestroyCause::Killed) ? DestroyCause::Killed
                                                                    : DestroyCause::Removed;
}

/// Writes the players and checks them, for SNAPSHOT.
void writePlayers(Writer &writer, const std::vector<PlayerState> &players)
{
    require(players.size() <= MaxPlayers, "A snapshot has at most 4 players");
    std::array<bool, MaxPlayers> seen{};
    writer.writeU8(static_cast<std::uint8_t>(players.size()));
    for (const PlayerState &player : players) {
        require(isSlot(player.slot), "A player slot is 0 to 3");
        require(!seen[player.slot], "A player slot is given once");
        seen[player.slot] = true;
        writer.writeU8(player.slot);
        writer.writeU8(player.lives);
        writer.writeU32(player.score);
    }
}

void writeEntity(Writer &writer, const EntityState &entity)
{
    require(entity.id != 0, "An entity id is never 0");
    require(entity.kind != EntityKind{0}, "An entity kind is never 0");
    require(isSlot(entity.owner) || entity.owner == NoOwner, "An owner is a player slot or NoOwner");
    writer.writeU32(entity.id);
    writer.writeU8(static_cast<std::uint8_t>(entity.kind));
    writer.writeU8(entity.owner);
    writer.writeI16(toFixed(entity.x));
    writer.writeI16(toFixed(entity.y));
    writer.writeI16(toFixed(entity.velocityX));
    writer.writeI16(toFixed(entity.velocityY));
    writer.writeU8(entity.health);
    writer.writeU8(static_cast<std::uint8_t>(entity.flags & KnownFlags));
}

EntityState readEntity(Reader &reader)
{
    EntityState entity;
    entity.id = reader.readU32();
    const std::uint8_t kind = reader.readU8();
    entity.kind = static_cast<EntityKind>(kind);
    entity.owner = reader.readU8();
    entity.x = fromFixed(reader.readI16());
    entity.y = fromFixed(reader.readI16());
    entity.velocityX = fromFixed(reader.readI16());
    entity.velocityY = fromFixed(reader.readI16());
    entity.health = reader.readU8();
    entity.flags = static_cast<std::uint8_t>(reader.readU8() & KnownFlags);
    if (entity.id == 0 || kind == 0) {
        reader.fail();
    }
    return entity;
}

/// The values of a byte.
constexpr std::size_t ByteValues = std::numeric_limits<std::uint8_t>::max() + 1;

/// Whether each byte is a kind of entity, indexed by the byte.
constexpr std::array<bool, ByteValues> KnownKinds = [] {
    std::array<bool, ByteValues> known{};
    for (const EntityKind kind :
         {EntityKind::PlayerShip, EntityKind::PlayerShot, EntityKind::PlayerChargedShot,
          EntityKind::BydoFlyer, EntityKind::BydoShooter, EntityKind::BydoTank, EntityKind::BydoBoss,
          EntityKind::BydoShot, EntityKind::BydoPlasmaShot}) {
        known[static_cast<std::uint8_t>(kind)] = true;
    }
    return known;
}();

/// The reasons of PLAYER_LEFT, indexed by their value.
constexpr std::array<LeaveReason, 3> LeaveReasons{LeaveReason::Quit, LeaveReason::TimedOut,
                                                  LeaveReason::LobbyClosed};

} // namespace

bool isKnown(EntityKind kind)
{
    return KnownKinds[static_cast<std::uint8_t>(kind)];
}

// --- ACCEPTED, PLAYER_JOINED, PLAYER_LEFT ---

std::vector<std::byte> encode(const AcceptedMessage &message)
{
    require(isSlot(message.slot), "A player slot is 0 to 3");
    require(message.tickRate != 0, "The tick rate is never 0");
    Writer writer(6);
    writer.writeU8(message.slot);
    writer.writeU8(message.tickRate);
    writer.writeU32(message.tick);
    return writer.take();
}

std::optional<AcceptedMessage> decodeAccepted(std::span<const std::byte> payload)
{
    Reader reader(payload);
    AcceptedMessage message;
    message.slot = reader.readU8();
    message.tickRate = reader.readU8();
    message.tick = reader.readU32();
    if (!isSlot(message.slot) || message.tickRate == 0) {
        reader.fail();
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

std::vector<std::byte> encode(const PlayerJoinedMessage &message)
{
    require(isSlot(message.slot), "A player slot is 0 to 3");
    require(!message.name.empty() && message.name.size() <= MaxPlayerNameLength,
            "A player name is 1 to 16 bytes");
    Writer writer(6 + message.name.size());
    writer.writeU8(message.slot);
    writer.writeU32(message.playerId);
    writer.writeString(message.name);
    return writer.take();
}

std::optional<PlayerJoinedMessage> decodePlayerJoined(std::span<const std::byte> payload)
{
    Reader reader(payload);
    PlayerJoinedMessage message;
    message.slot = reader.readU8();
    message.playerId = reader.readU32();
    message.name = reader.readString(1, MaxPlayerNameLength);
    if (!isSlot(message.slot)) {
        reader.fail();
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

std::vector<std::byte> encode(const PlayerLeftMessage &message)
{
    require(isSlot(message.slot), "A player slot is 0 to 3");
    Writer writer(2);
    writer.writeU8(message.slot);
    writer.writeU8(static_cast<std::uint8_t>(message.reason));
    return writer.take();
}

std::optional<PlayerLeftMessage> decodePlayerLeft(std::span<const std::byte> payload)
{
    Reader reader(payload);
    PlayerLeftMessage message;
    message.slot = reader.readU8();
    const std::uint8_t reason = reader.readU8();
    if (!isSlot(message.slot)) {
        reader.fail();
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    message.reason = reason < LeaveReasons.size() ? LeaveReasons[reason] : LeaveReason::Quit;
    return message;
}

// --- INPUT ---

std::vector<std::byte> encode(const InputMessage &message)
{
    require(!message.inputs.empty() && message.inputs.size() <= MaxInputsPerMessage,
            "An INPUT has 1 to 32 inputs");
    // The oldest input is the one of tick - (count - 1): never before tick 0
    require(message.inputs.size() <= std::uint64_t{message.tick} + 1, "An INPUT has no input before tick 0");
    Writer writer(5 + message.inputs.size());
    writer.writeU32(message.tick);
    writer.writeU8(static_cast<std::uint8_t>(message.inputs.size()));
    for (const PlayerInput input : message.inputs) {
        writer.writeU8(PlayerInput::fromByte(input.buttons).buttons);
    }
    return writer.take();
}

std::optional<InputMessage> decodeInput(std::span<const std::byte> payload)
{
    Reader reader(payload);
    InputMessage message;
    message.tick = reader.readU32();
    const std::uint8_t count = reader.readU8();
    if (count == 0 || count > MaxInputsPerMessage || count > std::uint64_t{message.tick} + 1) {
        reader.fail();
    }
    for (const std::byte input : reader.readBytes(count)) {
        message.inputs.push_back(PlayerInput::fromByte(static_cast<std::uint8_t>(input)));
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

// --- SNAPSHOT ---

std::size_t maxEntitiesPerPart(std::size_t playerCount)
{
    if (playerCount > MaxPlayers) {
        throw std::invalid_argument("A snapshot has at most 4 players");
    }
    // The header, the players, the entity count, then the entities
    return (rtype::network::MaxUnreliablePayloadSize - SnapshotHeaderSize - (PlayerStateSize * playerCount) -
            1) /
           EntityStateSize;
}

std::vector<std::byte> encode(const SnapshotPart &part)
{
    require(part.parts >= 1 && part.parts <= MaxSnapshotParts, "A snapshot has 1 to 16 parts");
    require(part.part < part.parts, "A part number is below the number of parts");
    if (part.players.size() <= MaxPlayers && part.entities.size() > maxEntitiesPerPart(part.players.size())) {
        throw std::length_error("A part of a snapshot does not fit in a datagram");
    }
    Writer writer(SnapshotHeaderSize + (PlayerStateSize * part.players.size()) + 1 +
                  (EntityStateSize * part.entities.size()));
    writer.writeU32(part.tick);
    writer.writeU32(part.lastInput);
    writer.writeU8(part.part);
    writer.writeU8(part.parts);
    writePlayers(writer, part.players);
    writer.writeU8(static_cast<std::uint8_t>(part.entities.size()));
    for (const EntityState &entity : part.entities) {
        writeEntity(writer, entity);
    }
    return writer.take();
}

std::optional<SnapshotPart> decodeSnapshotPart(std::span<const std::byte> payload)
{
    Reader reader(payload);
    SnapshotPart part;
    part.tick = reader.readU32();
    part.lastInput = reader.readU32();
    part.part = reader.readU8();
    part.parts = reader.readU8();
    if (part.parts == 0 || part.parts > MaxSnapshotParts || part.part >= part.parts) {
        reader.fail();
    }

    const std::uint8_t playerCount = reader.readU8();
    if (playerCount > MaxPlayers) {
        reader.fail();
    }
    std::array<bool, MaxPlayers> seen{};
    for (std::uint8_t i = 0; i < playerCount && reader.ok(); ++i) {
        PlayerState player;
        player.slot = reader.readU8();
        player.lives = reader.readU8();
        player.score = reader.readU32();
        if (!isSlot(player.slot) || seen[player.slot]) {
            reader.fail();
            break;
        }
        seen[player.slot] = true;
        part.players.push_back(player);
    }

    const std::uint8_t entityCount = reader.readU8();
    // Checked before reading them: a count larger than the data allocates nothing
    if (reader.remaining() != EntityStateSize * entityCount) {
        reader.fail();
    }
    for (std::uint8_t i = 0; i < entityCount && reader.ok(); ++i) {
        part.entities.push_back(readEntity(reader));
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return part;
}

std::vector<SnapshotPart> splitSnapshot(const Snapshot &snapshot, std::uint32_t lastInput)
{
    const std::size_t perPart = maxEntitiesPerPart(snapshot.players.size());
    const std::size_t needed = std::max<std::size_t>(1, (snapshot.entities.size() + perPart - 1) / perPart);
    const std::size_t count = std::min(needed, MaxSnapshotParts);

    std::vector<SnapshotPart> parts(count);
    for (std::size_t i = 0; i < count; ++i) {
        SnapshotPart &part = parts[i];
        part.tick = snapshot.tick;
        part.lastInput = lastInput;
        part.part = static_cast<std::uint8_t>(i);
        part.parts = static_cast<std::uint8_t>(count);
        part.players = snapshot.players;
        const std::size_t first = std::min(i * perPart, snapshot.entities.size());
        const std::size_t last = std::min(first + perPart, snapshot.entities.size());
        part.entities.assign(snapshot.entities.begin() + static_cast<std::ptrdiff_t>(first),
                             snapshot.entities.begin() + static_cast<std::ptrdiff_t>(last));
    }
    return parts;
}

// --- ENTITIES_DESTROYED, WAVE_STARTED, GAME_OVER ---

std::vector<std::byte> encode(const EntitiesDestroyedMessage &message)
{
    require(!message.entities.empty(), "An ENTITIES_DESTROYED has at least one entity");
    if (message.entities.size() > MaxDestroyedPerMessage) {
        throw std::length_error("An ENTITIES_DESTROYED does not fit in a datagram");
    }
    Writer writer(DestroyedHeaderSize + (DestroyedEntitySize * message.entities.size()));
    writer.writeU32(message.tick);
    writer.writeU8(static_cast<std::uint8_t>(message.entities.size()));
    for (const DestroyedEntity &entity : message.entities) {
        require(entity.id != 0, "An entity id is never 0");
        writer.writeU32(entity.id);
        writer.writeU8(static_cast<std::uint8_t>(entity.cause));
    }
    return writer.take();
}

std::optional<EntitiesDestroyedMessage> decodeEntitiesDestroyed(std::span<const std::byte> payload)
{
    Reader reader(payload);
    EntitiesDestroyedMessage message;
    message.tick = reader.readU32();
    const std::uint8_t count = reader.readU8();
    if (count == 0 || reader.remaining() != DestroyedEntitySize * count) {
        reader.fail();
    }
    for (std::uint8_t i = 0; i < count && reader.ok(); ++i) {
        DestroyedEntity entity;
        entity.id = reader.readU32();
        entity.cause = toCause(reader.readU8());
        if (entity.id == 0) {
            reader.fail();
        }
        message.entities.push_back(entity);
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

std::vector<EntitiesDestroyedMessage> destroyedMessages(const Snapshot &snapshot)
{
    std::vector<EntitiesDestroyedMessage> messages;
    for (std::size_t first = 0; first < snapshot.destroyed.size(); first += MaxDestroyedPerMessage) {
        const std::size_t last = std::min(first + MaxDestroyedPerMessage, snapshot.destroyed.size());
        messages.push_back({snapshot.tick,
                            {snapshot.destroyed.begin() + static_cast<std::ptrdiff_t>(first),
                             snapshot.destroyed.begin() + static_cast<std::ptrdiff_t>(last)}});
    }
    return messages;
}

std::vector<std::byte> encode(const WaveStartedMessage &message)
{
    require(message.wave != 0, "A wave is numbered from 1");
    Writer writer(2);
    writer.writeU16(message.wave);
    return writer.take();
}

std::optional<WaveStartedMessage> decodeWaveStarted(std::span<const std::byte> payload)
{
    Reader reader(payload);
    WaveStartedMessage message;
    message.wave = reader.readU16();
    if (message.wave == 0) {
        reader.fail();
    }
    if (!reader.finish()) {
        return std::nullopt;
    }
    return message;
}

std::vector<std::byte> encode(const GameOverMessage &message)
{
    Writer writer(1);
    writer.writeU8(static_cast<std::uint8_t>(message.outcome));
    return writer.take();
}

std::optional<GameOverMessage> decodeGameOver(std::span<const std::byte> payload)
{
    Reader reader(payload);
    const std::uint8_t outcome = reader.readU8();
    if (!reader.finish()) {
        return std::nullopt;
    }
    return GameOverMessage{outcome == static_cast<std::uint8_t>(GameOutcome::Victory) ? GameOutcome::Victory
                                                                                      : GameOutcome::Defeat};
}

} // namespace rtype::game
