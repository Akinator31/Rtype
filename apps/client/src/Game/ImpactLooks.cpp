#include "Game/ImpactLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <vector>

namespace rtype::client
{

namespace
{

/// Drawn after the shots and the ships.
constexpr int ImpactOrder = 30;

/// How far in front of the plane of the game the impacts are: closer to the camera than the wings of the
/// ships, which would hide them.
constexpr float ImpactDepth = 300.0f;

} // namespace

void ImpactLooks::setLook(game::EntityKind kind, const ImpactLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool ImpactLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

ecs::entity_t ImpactLooks::createImpact(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                                        ecs::Position position) const
{
    const ImpactLook &look = _looks.at(kind);
    const ecs::entity_t impact = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(impact, scene);
    ecs.addComponent<engine::Drawable>(impact, engine::Layer::World, ImpactOrder);
    // The camera is at negative z: the impact is in front of the game
    ecs.addComponent<engine::Model>(impact, look.model, render::Vector3{position.x, position.y, -ImpactDepth},
                                    LengthAxis, HalfTurn, look.scale, look.tint, 0.0f, std::vector<int>{},
                                    look.energy);
    // The ImpactSystem moves the time of the animation: it stops at its end instead of looping
    engine::ModelAnimation animation{look.animation};
    animation.speed = 0.0f;
    ecs.addComponent<engine::ModelAnimation>(impact, animation);
    ecs.addComponent<Impact>(impact, look.duration, 0.0f, look.tint.a);
    return impact;
}

} // namespace rtype::client
