#include "Game/ShotLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <vector>

namespace rtype::client
{

void ShotLooks::setLook(game::EntityKind kind, const ShotLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool ShotLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

void ShotLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                         const EntityLook &look) const
{
    const ShotLook &shot = _looks.at(state.kind);
    ecs.addComponent<engine::Model>(entity, shot.model, render::Vector3{}, LengthAxis, HalfTurn,
                                    look.box.width / shot.core.width, render::colors::White,
                                    shot.facesLeft ? HalfTurn : 0.0f, std::vector<int>{}, shot.energy);
    placeModel(ecs, entity, state);
}

void ShotLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const
{
    const render::Rectangle &core = _looks.at(state.kind).core;
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    model.position = {state.x - ((core.x + (core.width / 2.0f)) * model.scale),
                      state.y - ((core.y + (core.height / 2.0f)) * model.scale), 0.0f};
}

} // namespace rtype::client
