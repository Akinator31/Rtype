#pragma once

#include "rtype/game/Constants.hpp"

#include <cstdint>
#include <optional>
#include <string>
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
    /// A ship of the allies that come to the rescue against the final boss, offline only (RescueStage): it
    /// is never sent online.
    AllyShip = 0x04,
    BydoFlyer = 0x10,
    BydoShooter = 0x11,
    BydoTank = 0x12,
    BydoBoss = 0x1F,
    BydoShot = 0x40,
    BydoPlasmaShot = 0x41,
    /// An item the ships of the players pick up (ItemDefinition): its model is the item of the configuration
    /// whose index is EntityState::ship.
    Item = 0x60,
};

/// Why an entity was destroyed, for what the client shows.
enum class DestroyCause : std::uint8_t {
    /// Removed without effect: it left the game area, or its player left.
    Removed = 0,
    /// Destroyed in the game, a ship killed or a shot that hit: the client draws an explosion or an impact.
    Killed = 1,
};

/// How a game ended.
enum class GameOutcome : std::uint8_t {
    /// Every player lost all its lives.
    Defeat = 0,
    /// The players finished the level.
    Victory = 1,
};

/// Where the rescue against the final boss is, offline only (Rules::rescue, RescueSystem): when the last ship
/// of the last player is lost against the shogun, instead of the defeat.
enum class RescueStage : std::uint8_t {
    /// No rescue: the game goes on as usual.
    None = 0,
    /// The client plays the voice of the rescue; the shogun waits, guarding.
    Voice = 1,
    /// Five allies fly in, with a new ship for the player.
    Arrival = 2,
    /// The allies shoot at the shogun, which cannot be hurt.
    Assault = 3,
    /// The shogun dashes at every ship in turn, killing none.
    Dash = 4,
    /// The client asks the player to press Fire (Space) for the combined attack.
    Prompt = 5,
    /// Every ship fires at once: the shogun falls.
    Combined = 6,
    /// The shogun is dead: the victory comes a moment later.
    Finished = 7,
};

/// A player, in a snapshot.
struct PlayerState {
    /// 0 to 3: its place in the game, which sets the colour of its ship.
    std::uint8_t slot = 0;
    /// Lives left, 0 once out of the game.
    std::uint8_t lives = 0;
    std::uint32_t score = 0;
    /// Bydos it shot down.
    std::uint32_t kills = 0;
    /// Waves cleared, alive at their end, and how many in a row up to the last one.
    std::uint16_t wavesCleared = 0;
    std::uint16_t streak = 0;
    std::uint16_t bestStreak = 0;
    /// Experience earned in this game, 0 but online (Rules::endless).
    std::uint32_t xp = 0;

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
        /// A ship of a player turned nose up, or nose down, by Rules::aimAngle: it fires that way.
        AimingUp = 1 << 3,
        AimingDown = 1 << 4,
        /// A Bydo in a stance its behavior ordered (EnemyStance): behind its guard, striking with a blade,
        /// aiming its gun.
        Guarding = 1 << 5,
        Slashing = 1 << 6,
        Shooting = 1 << 7,
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
    /// For a shot (isShot()): ticks since it was fired, at most 255, so that a client knows the tick it was
    /// fired at. Written in the byte of the health, which a shot does not have.
    std::uint8_t age = 0;
    /// For a Bydo: the index of its ship among those of the configuration (ShipRegistry::getIndex()), so that
    /// the client draws its model at the size of its collider. For an item (EntityKind::Item): the index of
    /// its item among those of the configuration (ItemRegistry::getIndex()). For a shot (isShot()): the index
    /// of its projectile among those of the configuration (ProjectileRegistry::getIndex()), so that the
    /// client draws the model of the weapon that fired it. NoShip otherwise.
    std::uint8_t ship = NoShip;

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
    /// The music of the wave started (WaveDefinition::music), empty if it has none.
    std::string waveMusic;
    /// The game ended during the tick.
    std::optional<GameOutcome> gameOver;
    /// Where the rescue against the final boss is (offline only).
    RescueStage rescue = RescueStage::None;
    /// The ships the blade of the shogun went through since the previous snapshot, during the dashes of the
    /// rescue, by id (offline only): the client draws the cut on them.
    std::vector<std::uint32_t> struck;

    friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

} // namespace rtype::game
