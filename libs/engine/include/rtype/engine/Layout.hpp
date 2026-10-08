#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Metrics.hpp"
#include "rtype/engine/Style.hpp"
#include "rtype/engine/Widgets.hpp"

namespace rtype::engine
{

/// Stacks the children of the entity in a column centred on it: the widgets of a menu.
struct Column {
    /// Space between two children, in game units.
    float spacing = 16.0f;
};

/// Place of a child in its Column, from the top. The entity must have a Parent (the column) and a Hitbox.
struct ColumnItem {
    int index = 0;
};

/// Computes the Hitbox of every widget with a text that can get the focus or is in a Column: the size of its
/// text plus the padding of the style, times its Scale.
class HitboxSystem : public ecs::System
{
    const Metrics &_metrics;
    const InterfaceStyle &_style;
    ecs::ComponentCache<Focusable> &_focusables;
    ecs::ComponentCache<ColumnItem> &_items;
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<Scale> &_scales;
    ecs::ComponentCache<Hitbox> &_hitboxes;

public:
    /// @param metrics, style They must outlive the system.
    HitboxSystem(ecs::ComponentCaches &caches, const Metrics &metrics, const InterfaceStyle &style);

    void update(float elapsed) override;
};

/// Places the children of every Column: their LocalPosition is set so that they are stacked from the top by
/// index, centred on the column. Register it before the TransformSystem, which turns local positions into
/// positions.
class ColumnLayoutSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    ecs::ComponentCache<Column> &_columns;
    ecs::ComponentCache<ColumnItem> &_items;
    ecs::ComponentCache<ecs::Parent> &_parents;
    ecs::ComponentCache<Hitbox> &_hitboxes;
    ecs::ComponentCache<ecs::LocalPosition> &_locals;

public:
    ColumnLayoutSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs);

    void update(float elapsed) override;
};

} // namespace rtype::engine
