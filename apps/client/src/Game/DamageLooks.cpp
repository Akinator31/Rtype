#include "Game/DamageLooks.hpp"

#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <string>

namespace rtype::client
{

namespace
{

/// Drawn above the entities of the game drawn in 2D.
constexpr int DamageOrder = 40;

/// How far above the centre of the entity hit a number appears, in game units: on the top of its hull.
constexpr float DamageRaise = 30.0f;

/// A number appears larger, then shrinks to its size: it pops out of the hit.
constexpr float PopScale = 1.6f;
constexpr float PopDuration = 0.15f;

} // namespace

void DamageLooks::setLook(game::EntityKind kind, const DamageLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool DamageLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

ecs::entity_t DamageLooks::createNumber(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                                        ecs::Position position, std::uint8_t damage) const
{
    const DamageLook &look = _looks.at(kind);
    const ecs::entity_t number = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(number, scene);
    ecs.addComponent<engine::Drawable>(number, engine::Layer::World, DamageOrder);
    ecs.addComponent<ecs::Position>(number, position.x, position.y - DamageRaise);
    ecs.addComponent<engine::Text>(number, std::to_string(damage), look.font, look.size, look.color);
    ecs.addComponent<engine::Opacity>(number, 1.0f);
    ecs.addComponent<engine::Scale>(number, PopScale);
    ecs.addComponent<engine::Tween>(number, engine::Tween{.target = engine::TweenTarget::Scale,
                                                          .from = PopScale,
                                                          .to = 1.0f,
                                                          .duration = PopDuration,
                                                          .easing = engine::Easing::EaseOut});
    ecs.addComponent<DamageNumber>(number, Duration, 0.0f);
    return number;
}

} // namespace rtype::client
