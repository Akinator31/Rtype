#pragma once

#include "rtype/game/Messages.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace rtype::game
{

/// An entity a client removed, with its last state: where to draw its explosion.
struct RemovedEntity {
    EntityState last;
    DestroyCause cause = DestroyCause::Removed;

    friend bool operator==(const RemovedEntity &, const RemovedEntity &) = default;
};

/// The world as a client knows it from the messages of its game, kept as the protocol says
/// (docs/protocol/game.md, section 9.3): the parts of the snapshots, which can be lost, late or out of order,
/// and the destructions, which are reliable.
///
/// @code
/// if (const auto part = rtype::game::decodeSnapshotPart(payload)) {
///     world.apply(*part);
/// }
/// if (const auto destroyed = rtype::game::decodeEntitiesDestroyed(payload)) {
///     for (const rtype::game::RemovedEntity &removed : world.apply(*destroyed)) {
///         // an explosion at removed.last.x, removed.last.y if removed.cause is Killed
///     }
/// }
/// for (const auto &[id, entity] : world.entities()) { /* draw it */ }
/// @endcode
///
/// - A part older than the latest one applied is ignored; the other parts of that latest tick still apply.
/// - An entity is created or updated by the parts, and only removed by ENTITIES_DESTROYED: missing from
///   a part means its part was lost, not that it is gone.
/// - The ids destroyed are remembered until the snapshots reach the tick of their destruction, so that an
///   older part cannot bring them back.
/// - The entities of a kind this version does not know are skipped.
///
/// It keeps the latest state only: drawing in the past and interpolating come on top of it. Pure: no clock,
/// no network.
class RemoteWorld
{
    std::uint32_t _tick = 0;
    bool _received = false;
    std::uint32_t _lastInput = 0;
    std::vector<PlayerState> _players;
    std::map<std::uint32_t, EntityState> _entities;
    /// Ids destroyed at a tick the snapshots have not reached yet, with that tick.
    std::map<std::uint32_t, std::uint32_t> _destroyed;

public:
    /// Applies a part of a snapshot.
    /// @return false if it was late, hence ignored.
    bool apply(const SnapshotPart &part);

    /// Removes the entities destroyed, and remembers those destroyed after the latest snapshot.
    /// @return The entities removed, with their last state: those it did not know are not in it.
    std::vector<RemovedEntity> apply(const EntitiesDestroyedMessage &message);

    /// The tick of the latest snapshot applied, 0 before the first one.
    [[nodiscard]] std::uint32_t tick() const { return _tick; }
    /// Tells whether a snapshot was applied.
    [[nodiscard]] bool hasSnapshot() const { return _received; }
    /// The tick of the last input of this client that the game applied, from the latest snapshot.
    [[nodiscard]] std::uint32_t lastInput() const { return _lastInput; }
    /// The players of the latest snapshot: lives and scores.
    [[nodiscard]] const std::vector<PlayerState> &players() const { return _players; }
    /// The entities, by id.
    [[nodiscard]] const std::map<std::uint32_t, EntityState> &entities() const { return _entities; }
    /// An entity, or nullptr.
    [[nodiscard]] const EntityState *find(std::uint32_t id) const;
};

} // namespace rtype::game
