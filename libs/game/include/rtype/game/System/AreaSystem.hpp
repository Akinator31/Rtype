#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/game/Component/Components.hpp"
#include "rtype/game/GameContext.hpp"

namespace rtype::game
{

/// Removes the entities with LeavesArea once they are entirely outside the game area, by more than
/// Rules::areaMargin, and records them for the snapshot (DestroyCause::Removed). This also keeps every entity
/// in the positions the protocol can encode.
class AreaSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    GameContext &_context;
    ecs::ComponentCache<LeavesArea> &_leaving;
    ecs::ComponentCache<ecs::Position> &_positions;

public:
    AreaSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, GameContext &context);

    void update(float elapsed) override;
};

} // namespace rtype::game
