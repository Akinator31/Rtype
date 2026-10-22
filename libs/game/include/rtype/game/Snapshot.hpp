#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace rtype::game
{

/// What an entity is, hence what the client draws. The values are those of the protocol
/// (docs/protocol/game.md, section 9.1): `0x01`–`0x0F` players and their shots, `0x10`–`0x3F` Bydos,
/// `0x40`–`0x5F` Bydo shots, the rest for later kinds. Adding a kind changes nothing for the older clients,
/// which skip the kinds they do not know.
enum class EntityKind : std::uint8_t {
    PlayerShip = 0x01,
    PlayerShot = 0x02,
    PlayerChargedShot = 0x03,
    BydoFlyer = 0x10,
    BydoShooter = 0x11,
    BydoTank = 0x12,
    BydoBoss = 0x1F,
    BydoShot = 0x40,
    BydoPlasmaShot = 0x41,
};

/// Why an entity was destroyed, for what the client shows.
enum class DestroyCause : std::uint8_t {
    /// Removed without effect: it left the game area, or its player left.
    Removed = 0,
    /// Destroyed in the game: the client draws an explosion.
    Killed = 1,
};

/// How a game ended.
enum class GameOutcome : std::uint8_t {
    /// Every player lost all its lives.
    Defeat = 0,
    /// The players finished the level.
    Victory = 1,
};

/// A player, in a snapshot.
struct PlayerState {
    /// 0 to 3: its place in the game, which sets the colour of its ship.
    std::uint8_t slot = 0;
    /// Lives left, 0 once out of the game.
    std::uint8_t lives = 0;
    std::uint32_t score = 0;

    friend bool operator==(const PlayerState &, const PlayerState &) = default;
};

/// An entity, in a snapshot: everything the client needs to draw it.
struct EntityState {
    /// The flags of an entity.
    enum Flag : std::uint8_t {
        /// Just respawned: cannot be hit, drawn blinking.
        Invulnerable = 1 << 0,
        /// Charging a shot.
        Charging = 1 << 1,
        /// Hit since the previous snapshot: drawn flashing.
        Hit = 1 << 2,
    };

    /// Never 0, and never reused within a game: a client can keep it to recognize the entity.
    std::uint32_t id = 0;
    EntityKind kind = EntityKind::PlayerShip;
    /// The slot of the player it belongs to, or NoOwner.
    std::uint8_t owner = 0;
    /// Position of its centre, in game units.
    float x = 0.0f;
    float y = 0.0f;
    /// Velocity, in game units per second: lets the client extrapolate between snapshots.
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    /// Hit points left, for health bars; 0 for kinds without.
    std::uint8_t health = 0;
    std::uint8_t flags = 0;

    friend bool operator==(const EntityState &, const EntityState &) = default;
};

/// An entity destroyed during a tick.
struct DestroyedEntity {
    std::uint32_t id = 0;
    DestroyCause cause = DestroyCause::Removed;

    friend bool operator==(const DestroyedEntity &, const DestroyedEntity &) = default;
};

/// The world at the end of a tick, as the game sees it: what the client draws, offline (LocalSession) or
/// online (decoded from SNAPSHOT and the events, docs/protocol/game.md, section 9).
struct Snapshot {
    /// The tick it describes, 1 after the first step.
    std::uint32_t tick = 0;
    /// The players, by slot.
    std::vector<PlayerState> players;
    /// The entities alive at the end of the tick, by id.
    std::vector<EntityState> entities;

    /// The entities destroyed since the previous snapshot: they are not in `entities` any more.
    std::vector<DestroyedEntity> destroyed;
    /// A wave started during the tick: its number, from 1.
    std::optional<std::uint16_t> waveStarted;
    /// The game ended during the tick.
    std::optional<GameOutcome> gameOver;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

} // namespace rtype::game
