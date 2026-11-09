#include "Visuals.hpp"

#include "rtype/ecs/Hierarchy.hpp"

namespace orbit
{

namespace ecs = rtype::ecs;
namespace engine = rtype::engine;

ecs::entity_t spawnDrawn(ecs::Ecs &ecs, engine::SceneId scene, float x, float y, engine::Layer layer,
                         int order)
{
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<ecs::Position>(entity, x, y);
    ecs.addComponent<engine::Drawable>(entity, layer, order);
    return entity;
}

void popIn(ecs::Ecs &ecs, ecs::entity_t entity, float from, float duration)
{
    ecs.addComponent<engine::Tween>(entity, engine::Tween{.target = engine::TweenTarget::Scale,
                                                          .from = from,
                                                          .to = 1.0f,
                                                          .duration = duration,
                                                          .easing = engine::Easing::EaseOutBack});
}

engine::Tween pingPong(engine::TweenTarget target, float from, float to, float duration)
{
    return {.target = target, .from = from, .to = to, .duration = duration, .pingPong = true};
}

ecs::entity_t styledText(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const std::string &text,
                         engine::Anchor anchor, float size, rtype::render::Color color)
{
    const ecs::entity_t entity = interface.text(text, anchor);
    engine::Text &look = ecs.getComponent<engine::Text>(entity);
    look.size = size;
    look.color = color;
    return entity;
}

} // namespace orbit
