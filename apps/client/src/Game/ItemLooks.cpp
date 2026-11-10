#include "Game/ItemLooks.hpp"

#include "Game/GameView.hpp"
#include "Game/ImpactLooks.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

namespace
{

/// How far an item floats up and down, in game units, and how long a float takes, in seconds.
constexpr float FloatHeight = 5.0f;
constexpr float FloatPeriod = 2.0f;

/// The collect is drawn after the items and the ships: the shock wave goes over the ship that picked it up.
constexpr int CollectOrder = 30;

/// Where the origin of a model goes so that the centre of its box is at (x, y). The models face the camera
/// once turned by a half turn around their length (GameView.hpp): y and z change sign.
render::Vector3 placeCentre(const render::Vector3 &centre, float scale, float x, float y)
{
    return {x - (centre.x * scale), y + (centre.y * scale), centre.z * scale};
}

} // namespace

void ItemLooks::setLook(std::uint8_t item, const ItemLook &look)
{
    _looks.insert_or_assign(item, look);
}

bool ItemLooks::hasLook(std::uint8_t item) const
{
    return _looks.contains(item);
}

void ItemLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const
{
    const ItemLook &look = _looks.at(state.ship);
    const render::Box3 &bounds = look.bounds;
    const float width = bounds.max.x - bounds.min.x;
    const float height = bounds.max.y - bounds.min.y;
    // The model fits in the box of its collider
    const float fit =
        std::min(width > 0.0f ? look.size.x / width : 1.0f, height > 0.0f ? look.size.y / height : 1.0f);
    const render::Vector3 centre{(bounds.min.x + bounds.max.x) / 2.0f, (bounds.min.y + bounds.max.y) / 2.0f,
                                 (bounds.min.z + bounds.max.z) / 2.0f};
    ecs.addComponent<engine::Model>(entity, look.model, render::Vector3{state.x, state.y, 0.0f}, LengthAxis,
                                    HalfTurn, 0.0f);
    if (look.idle) {
        // Each item at its own point of the loop, so that those side by side do not pulse together
        engine::ModelAnimation animation{look.idle->index};
        animation.time = std::fmod(static_cast<float>(state.id) * 0.37f, look.idle->duration);
        ecs.addComponent<engine::ModelAnimation>(entity, animation);
    }
    ecs.addComponent<ItemModel>(entity, state.ship, fit, 0.0f, state.x, state.y, centre);
    placeModel(ecs, entity, 0.0f);
}

void ItemLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state)
{
    ItemModel &item = ecs.getComponent<ItemModel>(entity);
    item.x = state.x;
    item.y = state.y;
}

void ItemLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity, float elapsed)
{
    ItemModel &item = ecs.getComponent<ItemModel>(entity);
    item.age += elapsed;
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    const float grown = item.age >= AppearDuration
                            ? 1.0f
                            : engine::ease(engine::Easing::EaseOutBack, item.age / AppearDuration);
    model.scale = item.scale * std::max(0.0f, grown);
    const float floating = FloatHeight * std::sin(2.0f * std::numbers::pi_v<float> * item.age / FloatPeriod);
    model.position = placeCentre(item.centre, model.scale, item.x, item.y + floating);
}

std::optional<ecs::entity_t> ItemLooks::createCollect(ecs::Ecs &ecs, engine::SceneId scene,
                                                      ecs::entity_t entity) const
{
    const ItemModel &item = ecs.getComponent<ItemModel>(entity);
    const auto found = _looks.find(item.item);
    if (found == _looks.end() || !found->second.collect) {
        return std::nullopt;
    }
    const ItemLook &look = found->second;
    const engine::Model &drawn = ecs.getComponent<engine::Model>(entity);
    const ecs::entity_t collect = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(collect, scene);
    ecs.addComponent<engine::Drawable>(collect, engine::Layer::World, CollectOrder);
    ecs.addComponent<engine::Model>(collect, look.model, drawn.position, LengthAxis, HalfTurn, item.scale);
    ecs.getComponent<engine::Model>(collect).position =
        placeCentre(item.centre, item.scale, drawn.position.x + (item.centre.x * drawn.scale),
                    drawn.position.y - (item.centre.y * drawn.scale));
    // The ImpactSystem moves the time of the animation: it stops at its end instead of looping
    engine::ModelAnimation animation{look.collect->index};
    animation.speed = 0.0f;
    ecs.addComponent<engine::ModelAnimation>(collect, animation);
    ecs.addComponent<Impact>(collect, look.collect->duration, 0.0f, std::uint8_t{255});
    return collect;
}

} // namespace rtype::client
