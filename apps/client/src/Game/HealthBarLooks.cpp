#include "Game/HealthBarLooks.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"

namespace rtype::client
{

namespace
{

/// Drawn above the entities of the game drawn in 2D, under the damage numbers.
constexpr int HealthBarOrder = 35;

} // namespace

void HealthBarLooks::setLook(game::EntityKind kind, const HealthBarLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool HealthBarLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

ecs::entity_t HealthBarLooks::createBar(ecs::Ecs &ecs, engine::SceneId scene, game::EntityKind kind,
                                        ecs::entity_t entity) const
{
    const HealthBarLook &look = _looks.at(kind);
    const ecs::entity_t bar = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(bar, scene);
    ecs.addComponent<engine::Drawable>(bar, engine::Layer::World, HealthBarOrder);
    ecs::attach(ecs, bar, entity, {0.0f, -look.raise});
    ecs.addComponent<engine::Gauge>(
        bar, engine::Gauge{"", [&ecs, bar] { return ecs.getComponent<HealthBar>(bar).shown; }, look.width,
                           look.low});
    ecs.addComponent<engine::Opacity>(bar, 1.0f);
    ecs.addComponent<HealthBar>(bar);
    return bar;
}

} // namespace rtype::client
