#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/Snapshot.hpp"

#include <unordered_map>

namespace rtype::client
{

/// How the health bar of the entities of a kind looks, shown above them for a while when they are hit: the
/// gauge of the interface (engine::Gauge), without a caption.
struct HealthBarLook {
    /// Length of the bar, in game units.
    float width = 100.0f;
    /// How far above the centre of the entity the bar is, in game units: over its hull.
    float raise = 50.0f;
    /// Share of the hit points at or below which the bar turns red.
    float low = 0.3f;
};

/// The health bar of an entity, attached to it: an engine::Gauge that shows `shown`. The HealthBarSystem
/// slides it down to the hit points left, and fades it out a while after the last hit.
struct HealthBar {
    /// Share of the hit points left, from 0 to 1.
    float health = 1.0f;
    /// Share the gauge shows, which catches up with `health`: the bar is seen going down.
    float shown = 1.0f;
    /// Seconds since the last hit.
    float time = 0.0f;
};

/// The HealthBarLook of each kind of entity whose health is shown, and the bars they get in the game scene
/// (docs/architecture/client-game.md, "Health bars").
class HealthBarLooks
{
    std::unordered_map<game::EntityKind, HealthBarLook> _looks;

public:
    void setLook(game::EntityKind kind, const HealthBarLook &look);
    /// The health of the entities of this kind is shown.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Creates the health bar of an entity of this kind, attached above it, full and shown: it goes with the
    /// entity.
    ecs::entity_t createBar(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                            ecs::entity_t entity) const;
};

} // namespace rtype::client
