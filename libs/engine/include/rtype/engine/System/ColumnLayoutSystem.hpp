#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"

namespace rtype::engine
{

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
