#pragma once

#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <map>

namespace rtype::client
{

/// Draws the shots of the player online in the time of its ship (docs/architecture/client-online.md).
///
/// The ship of the player is predicted: drawn at the tick of its newest input, ahead of the game. The rest of
/// the world is drawn in the past, between two snapshots. Its own shots, drawn with the rest, would trail
/// behind the ship by a round trip and the delay of the interpolation: far behind its nose when it flies
/// forward, out of line with it when it turns.
///
/// @code
/// shots.destroyed(id, tick);                                // ENTITIES_DESTROYED
/// shots.apply(snapshot, slot, drawnTick, inputTick);        // each frame, on the world drawn
/// @endcode
///
/// - **Retimed**: each shot of the player is moved on along its velocity, from the drawn tick to the tick of
///   the ship. Its trail follows the ship as offline.
/// - **Its end**: a shot moved on would fly through what it hit, until the drawn tick reaches its
///   destruction. Once its destruction is known and the ship is past it, the shot is drawn one last frame
///   where it was destroyed, for the impact, then no more.
///
/// Pure: no clock, no network.
class ShotPrediction
{
private:
    struct End {
        std::uint32_t tick = 0;
        /// Drawn once where it ended: it is no longer drawn.
        bool shown = false;
    };

    /// The shots of the player destroyed, by id, until the drawn tick passes their destruction.
    std::map<std::uint32_t, End> _ends;

public:
    /// Whether an entity is a shot of the player of this slot.
    [[nodiscard]] static bool isShotOf(const game::EntityState &entity, std::uint8_t slot);

    /// A shot of the player destroyed at a tick (ENTITIES_DESTROYED).
    void destroyed(std::uint32_t id, std::uint32_t tick);

    /// Moves the shots of the player in the world drawn to the time of its ship, and leaves out those past
    /// their end.
    /// @param slot The slot of the player.
    /// @param drawn The tick the world is drawn at (Interpolation::drawnTick()).
    /// @param ship The tick its ship is drawn at: the one of its newest input.
    void apply(game::Snapshot &snapshot, std::uint8_t slot, double drawn, double ship);

    /// Destructions remembered.
    [[nodiscard]] std::size_t size() const { return _ends.size(); }
};

} // namespace rtype::client
