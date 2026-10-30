#include "Game/BossLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <cmath>

namespace rtype::client
{

namespace
{

/// Above this speed, in game units per second, a boss without a stance flies; below, it stands.
constexpr float FlyingSpeed = 60.0f;
/// Seconds a boss fades from an animation to the next.
constexpr float PoseFade = 0.25f;

/// The animation of what a boss does: its stance, or flying, or standing.
int chooseAnimation(const BossAnimations &animations, const game::EntityState &state)
{
    if ((state.flags & game::EntityState::Guarding) != 0) {
        return animations.guard;
    }
    if ((state.flags & game::EntityState::Slashing) != 0) {
        return animations.slash;
    }
    if ((state.flags & game::EntityState::Shooting) != 0) {
        return animations.shoot;
    }
    if (std::hypot(state.velocityX, state.velocityY) > FlyingSpeed) {
        return animations.fly;
    }
    return animations.idle;
}

} // namespace

BossLook loadBossLook(render::ResourceManager &resources, const std::string &path, render::Rectangle hull,
                      float spin)
{
    const render::ModelId model = resources.loadModel(path);
    return {model,
            {resources.getModelAnimation(model, "Idle"), resources.getModelAnimation(model, "Fly"),
             resources.getModelAnimation(model, "Slash"), resources.getModelAnimation(model, "Guard"),
             resources.getModelAnimation(model, "Shoot")},
            hull,
            spin};
}

void BossLooks::setLook(game::EntityKind kind, const BossLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool BossLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

void BossLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                         const EntityLook &look) const
{
    const BossLook &boss = _looks.at(state.kind);
    const render::Rectangle &hull = boss.hull;
    // A half turn around its length puts its top up on the screen, as for the ships
    ecs.addComponent<engine::Model>(entity, boss.model, render::Vector3{}, LengthAxis, HalfTurn,
                                    look.box.width / hull.width, render::colors::White, boss.spin);
    ecs.addComponent<engine::ModelAnimation>(entity, chooseAnimation(boss.animations, state));
    ecs.addComponent<BossModel>(entity,
                                render::Vector2{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)});
    placeModel(ecs, entity);
}

void BossLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const
{
    engine::ModelAnimation &animation = ecs.getComponent<engine::ModelAnimation>(entity);
    const int next = chooseAnimation(_looks.at(state.kind).animations, state);
    if (next != animation.animation) {
        engine::switchModelAnimation(animation, next, PoseFade);
    }
}

void BossLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity)
{
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    const render::Vector2 &centre = ecs.getComponent<BossModel>(entity).hullCentre;
    const ecs::Position &position = ecs.getComponent<ecs::Position>(entity);
    model.position = {position.x - (centre.x * model.scale), position.y - (centre.y * model.scale), 0.0f};
}

} // namespace rtype::client
