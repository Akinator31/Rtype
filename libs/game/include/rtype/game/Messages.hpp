#pragma once

#include "rtype/game/Constants.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/voice/Backend.hpp"
#include "rtype/voice/Format.hpp"
#include "rtype/voice/Status.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The messages of a game session that libs/network does not handle itself: their payloads, written and read
/// as the game protocol says (docs/protocol/game.md, sections 8 and 9). The sessions of libs/network
/// (ClientSession, SessionHost) carry them with their type; here they become bytes and back.
///
/// @code
/// // Game: one SNAPSHOT per part, for one client
/// for (const rtype::game::SnapshotPart &part : rtype::game::splitSnapshot(simulation.snapshot(), lastInput))
/// {
///     host.send(session, rtype::game::message_type::Snapshot, rtype::game::encode(part),
///               rtype::network::Delivery::Unreliable);
/// }
///
/// // Client: in the message handler of its ClientSession
/// if (type == rtype::game::message_type::Snapshot) {
///     if (const auto part = rtype::game::decodeSnapshotPart(payload)) {
///         world.apply(*part);
///     }
/// }
/// @endcode
///
/// The encoders throw std::invalid_argument for a value the protocol forbids: a bug of the sender. The
/// decoders never throw: they give nothing for a malformed payload, which the receiver drops. They are pure,
/// so that they can be fuzzed.
namespace rtype::game
{

/// The types of the messages, as given to and by the session.
namespace message_type
{
/// Game → client: the answer to `CONNECT` (rtype::network::session_message::Accepted), with AcceptedMessage.
inline constexpr std::uint8_t Accepted = 0x02;
/// Game → client: a player connected, or was there when this client connected.
inline constexpr std::uint8_t PlayerJoined = 0x04;
/// Game → client: the session of a player closed.
inline constexpr std::uint8_t PlayerLeft = 0x05;
/// Client → game: the inputs of the last ticks.
inline constexpr std::uint8_t Input = 0x40;
/// Client → game: one frame of the voice of the player.
inline constexpr std::uint8_t Voice = 0x41;
/// Client → game: what carries the voice of the player.
inline constexpr std::uint8_t VoiceState = 0x42;
/// Client → game: whether the player is muted, deafened, without a microphone.
inline constexpr std::uint8_t VoiceStatus = 0x43;
/// Game → client: one part of a snapshot.
inline constexpr std::uint8_t Snapshot = 0x80;
/// Game → client: the entities destroyed during a tick.
inline constexpr std::uint8_t EntitiesDestroyed = 0x81;
/// Game → client: a wave of Bydos started.
inline constexpr std::uint8_t WaveStarted = 0x82;
/// Game → client: the game ended.
inline constexpr std::uint8_t GameOver = 0x83;
/// Game → client: one frame of the voice of another player.
inline constexpr std::uint8_t VoiceFrame = 0x84;
/// Game → client: what carries the voice of each player.
inline constexpr std::uint8_t VoiceRoster = 0x85;
/// Game → client: what each player tells about its voice.
inline constexpr std::uint8_t VoiceStatuses = 0x86;
} // namespace message_type

/// Most inputs in one INPUT message.
inline constexpr std::size_t MaxInputsPerMessage = 32;
/// Inputs a client sends in each INPUT: the last 8 ticks, so that a lost datagram loses no input.
inline constexpr std::size_t InputRedundancy = 8;
/// Most parts of one snapshot.
inline constexpr std::size_t MaxSnapshotParts = 16;
/// The weapon of the ship of a client, in its snapshots (docs/protocol/game.md, section 9.3): what its
/// prediction of the shots starts again from. All 0 without a ship.
struct WeaponState {
    /// Ticks before it can fire again.
    std::uint16_t cooldown = 0;
    /// Shots left in the magazine.
    std::uint16_t ammo = 0;
    /// Ticks before the magazine is full again, 0 when it is not reloading.
    std::uint16_t reloadLeft = 0;

    friend bool operator==(const WeaponState &, const WeaponState &) = default;
};

/// The input lead of a SNAPSHOT when the game received no INPUT from this client since the previous snapshot.
inline constexpr std::int8_t NoInputLead = -128;
/// Most entities in one ENTITIES_DESTROYED that still fits in a datagram (checked against libs/network).
inline constexpr std::size_t MaxDestroyedPerMessage = 228;
/// Longest name of a player, in bytes of UTF-8.
inline constexpr std::size_t MaxPlayerNameLength = 16;
/// Most bytes of the music of a wave in WAVE_STARTED.
inline constexpr std::size_t MaxWaveMusicLength = 128;

/// `ACCEPTED`: the place of the client in the game.
struct AcceptedMessage {
    /// 0 to 3.
    std::uint8_t slot = 0;
    /// Ticks per second of the simulation, never 0.
    std::uint8_t tickRate = TickRate;
    /// The tick the game is at.
    std::uint32_t tick = 0;
    /// The game carries the voice (version 5, `[voice] enabled` of the server): without it, the client hides
    /// the voice and sends none.
    bool voice = true;

    friend bool operator==(const AcceptedMessage &, const AcceptedMessage &) = default;
};

/// `PLAYER_JOINED`.
struct PlayerJoinedMessage {
    /// 0 to 3.
    std::uint8_t slot = 0;
    /// The id the master gave the player.
    std::uint32_t playerId = 0;
    /// 1 to MaxPlayerNameLength bytes of UTF-8, without control characters.
    std::string name;
    /// The id of its account on the server, 0 for a guest (version 4): the other clients keep their choices
    /// for this player (its volume, muted) under it.
    std::uint64_t accountId = 0;

    friend bool operator==(const PlayerJoinedMessage &, const PlayerJoinedMessage &) = default;
};

/// Why a player left, in `PLAYER_LEFT`.
enum class LeaveReason : std::uint8_t {
    /// The player left the game. Also any unknown reason received.
    Quit = 0,
    /// Nothing received from the player for 5 seconds: its client crashed or lost the network.
    TimedOut = 1,
    /// The connection of the player to the master closed.
    LobbyClosed = 2,
};

/// `PLAYER_LEFT`.
struct PlayerLeftMessage {
    /// 0 to 3.
    std::uint8_t slot = 0;
    LeaveReason reason = LeaveReason::Quit;

    friend bool operator==(const PlayerLeftMessage &, const PlayerLeftMessage &) = default;
};

/// `INPUT`: the inputs of the last ticks, newest first (docs/protocol/game.md, section 9.2).
struct InputMessage {
    /// The tick of the first input, the newest.
    std::uint32_t tick = 0;
    /// The input of `tick`, then of `tick - 1`, and so on: 1 to MaxInputsPerMessage, at most `tick + 1`.
    std::vector<PlayerInput> inputs;

    friend bool operator==(const InputMessage &, const InputMessage &) = default;
};

/// `SNAPSHOT`: one part of the world at a tick (docs/protocol/game.md, section 9.3). Each part has the
/// players; the entities are shared between the parts.
struct SnapshotPart {
    /// The tick of the world described.
    std::uint32_t tick = 0;
    /// The tick of the last input of this client that the game applied, 0 if none.
    std::uint32_t lastInput = 0;
    /// The number of this part, from 0, below `parts`.
    std::uint8_t part = 0;
    /// The number of parts of this snapshot, 1 to MaxSnapshotParts.
    std::uint8_t parts = 1;
    /// 0 to 4 players, each slot once.
    std::vector<PlayerState> players;
    /// The entities of this part. Decoded, they may have kinds this client does not know (isKnown()).
    std::vector<EntityState> entities;
    /// How early the inputs of this client arrive (docs/protocol/game.md, section 9.2), written after the
    /// last input: the smallest, over the INPUT messages received since the previous snapshot, of the tick of
    /// their newest input minus the last tick simulated when it arrived, within -127 to 127. 1 is just in
    /// time, 0 or less too late. NoInputLead if none arrived.
    std::int8_t inputLead = NoInputLead;
    /// The weapon of the ship of this client at `tick`, written after the input lead.
    WeaponState weapon;

    friend bool operator==(const SnapshotPart &, const SnapshotPart &) = default;
};

/// `ENTITIES_DESTROYED`.
struct EntitiesDestroyedMessage {
    /// The tick of the destruction.
    std::uint32_t tick = 0;
    /// 1 to MaxDestroyedPerMessage entities.
    std::vector<DestroyedEntity> entities;

    friend bool operator==(const EntitiesDestroyedMessage &, const EntitiesDestroyedMessage &) = default;
};

/// `WAVE_STARTED`.
struct WaveStartedMessage {
    /// From 1.
    std::uint16_t wave = 1;
    /// 0 to MaxWaveMusicLength bytes of UTF-8, without control characters: the music to play, a path
    /// relative to the assets of the client. Empty: none.
    std::string music;

    friend bool operator==(const WaveStartedMessage &, const WaveStartedMessage &) = default;
};

/// `GAME_OVER`.
struct GameOverMessage {
    GameOutcome outcome = GameOutcome::Defeat;

    friend bool operator==(const GameOverMessage &, const GameOverMessage &) = default;
};

/// `VOICE`: one frame of the voice of the player, 20 ms of Opus (docs/protocol/game.md, section 9.4).
struct VoiceMessage {
    /// The number of the frame: one more every 20 ms while the player talks, wrapping after 65535.
    std::uint16_t sequence = 0;
    /// The last frame of a talk spurt: the player stopped talking after it.
    bool last = false;
    /// 1 to voice::MaxFrameBytes bytes of Opus. The game does not decode them.
    std::vector<std::byte> frame;

    friend bool operator==(const VoiceMessage &, const VoiceMessage &) = default;
};

/// `VOICE_FRAME`: a frame of `VOICE`, forwarded by the game to the other players.
struct VoiceFrameMessage {
    /// The slot of the player who talks, 0 to 3.
    std::uint8_t slot = 0;
    /// As sent in `VOICE`.
    std::uint16_t sequence = 0;
    bool last = false;
    std::vector<std::byte> frame;

    friend bool operator==(const VoiceFrameMessage &, const VoiceFrameMessage &) = default;
};

/// `VOICE_STATE`: what carries the voice of the player (ADR 0035).
struct VoiceStateMessage {
    voice::Backend backend = voice::Backend::None;

    friend bool operator==(const VoiceStateMessage &, const VoiceStateMessage &) = default;
};

/// `VOICE_ROSTER`: what carries the voice of each player, by slot; None for a slot without a player.
struct VoiceRosterMessage {
    std::array<voice::Backend, MaxPlayers> backends{};

    friend bool operator==(const VoiceRosterMessage &, const VoiceRosterMessage &) = default;
};

/// `VOICE_STATUS`: what the player tells about its voice.
struct VoiceStatusMessage {
    voice::Status status{};

    friend bool operator==(const VoiceStatusMessage &, const VoiceStatusMessage &) = default;
};

/// `VOICE_STATUSES`: what each player tells about its voice, by slot; nothing set for a slot without a
/// player.
struct VoiceStatusesMessage {
    std::array<voice::Status, MaxPlayers> statuses{};

    friend bool operator==(const VoiceStatusesMessage &, const VoiceStatusesMessage &) = default;
};

/// Tells whether a kind is one this version knows: a client skips the others (docs/protocol/game.md,
/// section 9.1).
[[nodiscard]] bool isKnown(EntityKind kind);

/// Tells whether a kind is a shot, whose entity states give their age instead of their health: the shots of
/// the players (`0x02`, `0x03`) and those of the Bydos (`0x40`–`0x5F`).
[[nodiscard]] bool isShot(EntityKind kind);

/// The payloads of the messages. Positions and velocities are written in eighths of a unit, rounded and kept
/// within what an i16 holds; the reserved bits of inputs and flags are written as 0.
/// @throws std::invalid_argument for a value the protocol forbids (a slot above 3, no input, an id of 0...).
[[nodiscard]] std::vector<std::byte> encode(const AcceptedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerJoinedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const PlayerLeftMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const InputMessage &message);
/// @throws std::length_error if the part does not fit in a datagram: use splitSnapshot().
[[nodiscard]] std::vector<std::byte> encode(const SnapshotPart &part);
/// @throws std::length_error if the entities do not fit in a datagram: use destroyedMessages().
[[nodiscard]] std::vector<std::byte> encode(const EntitiesDestroyedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const WaveStartedMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const GameOverMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceFrameMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceStateMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceRosterMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceStatusMessage &message);
[[nodiscard]] std::vector<std::byte> encode(const VoiceStatusesMessage &message);

/// The messages read from their payloads, strictly: nothing for every case the protocol calls malformed, or
/// for bytes left after the last field. Unknown reasons, causes, outcomes and backends are read as the
/// protocol says (Quit, Removed, Defeat, None), and the reserved bits are dropped.
[[nodiscard]] std::optional<AcceptedMessage> decodeAccepted(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerJoinedMessage> decodePlayerJoined(std::span<const std::byte> payload);
[[nodiscard]] std::optional<PlayerLeftMessage> decodePlayerLeft(std::span<const std::byte> payload);
[[nodiscard]] std::optional<InputMessage> decodeInput(std::span<const std::byte> payload);
[[nodiscard]] std::optional<SnapshotPart> decodeSnapshotPart(std::span<const std::byte> payload);
[[nodiscard]] std::optional<EntitiesDestroyedMessage>
decodeEntitiesDestroyed(std::span<const std::byte> payload);
[[nodiscard]] std::optional<WaveStartedMessage> decodeWaveStarted(std::span<const std::byte> payload);
[[nodiscard]] std::optional<GameOverMessage> decodeGameOver(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceMessage> decodeVoice(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceFrameMessage> decodeVoiceFrame(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceStateMessage> decodeVoiceState(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceRosterMessage> decodeVoiceRoster(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceStatusMessage> decodeVoiceStatus(std::span<const std::byte> payload);
[[nodiscard]] std::optional<VoiceStatusesMessage> decodeVoiceStatuses(std::span<const std::byte> payload);

/// Most entities in one part of a snapshot with this many players: 69 with four players.
[[nodiscard]] std::size_t maxEntitiesPerPart(std::size_t playerCount);

/// Splits a snapshot in the parts that fit in datagrams, each with the tick and the players, for one client.
/// A snapshot without entities is one part. Beyond MaxSnapshotParts full parts (1104 entities with four
/// players), the last entities are left out: the rules must never have that many.
/// @param lastInput The tick of the last input of this client that the game applied, 0 if none.
/// @param inputLead How early the inputs of this client arrive (SnapshotPart::inputLead).
/// @param weapon The weapon of the ship of this client.
[[nodiscard]] std::vector<SnapshotPart> splitSnapshot(const Snapshot &snapshot, std::uint32_t lastInput,
                                                      std::int8_t inputLead = NoInputLead,
                                                      WeaponState weapon = {});

/// The ENTITIES_DESTROYED messages of a snapshot: none if nothing was destroyed, several with the same tick
/// if they do not fit in one datagram.
[[nodiscard]] std::vector<EntitiesDestroyedMessage> destroyedMessages(const Snapshot &snapshot);

} // namespace rtype::game
