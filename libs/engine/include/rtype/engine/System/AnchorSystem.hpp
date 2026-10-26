#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/Metrics.hpp"

#include <optional>

namespace rtype::engine
{

/// Writes the Position of the entities with an Anchor, every frame, from the part of the game shown on the
/// screen (Metrics::visibleArea(), so that anchored elements follow the edges of any screen) and the size of
/// the entity: its Hitbox at scale 1 if it has one (a widget, with its padding), otherwise its Text, Sprite
/// Box or Extent. The size does not include the Scale, so that an entity does not move when it bounces.
///
/// Register it after the systems that change the texts and the hitboxes (BoundTextSystem, HitboxSystem), and
/// before the TransformSystem.
class AnchorSystem : public ecs::System
{
    const Metrics &_metrics;
    ecs::ComponentCache<Anchor> &_anchors;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<Hitbox> &_hitboxes;
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<Sprite> &_sprites;
    ecs::ComponentCache<Box> &_boxes;
    ecs::ComponentCache<Extent> &_extents;

    [[nodiscard]] std::optional<render::Vector2> sizeOf(ecs::entity_t entity) const;

public:
    /// @param metrics It must outlive the system.
    AnchorSystem(ecs::ComponentCaches &caches, const Metrics &metrics);

    void update(float elapsed) override;
};

} // namespace rtype::engine
