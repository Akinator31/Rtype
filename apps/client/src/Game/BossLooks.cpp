#include "Game/BossLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/Constants.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

/// Above this speed, in game units per second, a boss without a stance flies; below, it stands.
constexpr float FlyingSpeed = 60.0f;
/// Seconds a boss fades from an animation to the next.
constexpr float PoseFade = 0.25f;
/// Above this speed, in game units per second, a boss with its blade out is dashing, not stepping back.
constexpr float DashSpeed = 400.0f;
/// How far to the sides it is heard, as for the other entities (EntitySounds).
constexpr float PanSpread = 0.6f;

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
    // Created first: the caches may grow with the entities
    std::vector<ecs::entity_t> sources;
    for (std::size_t sound = 0; sound < boss.slashSounds.size(); ++sound) {
        const ecs::entity_t source = ecs.createEntity();
        ecs.addComponent<engine::SceneMember>(source, ecs.getComponent<engine::SceneMember>(entity).scene);
        ecs::attach(ecs, source, entity);
        sources.push_back(source);
    }
    ecs.addComponent<BossModel>(entity,
                                render::Vector2{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)},
                                false, std::move(sources));
    placeModel(ecs, entity);
}

void BossLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const
{
    engine::ModelAnimation &animation = ecs.getComponent<engine::ModelAnimation>(entity);
    const BossLook &boss = _looks.at(state.kind);
    const int next = chooseAnimation(boss.animations, state);
    if (next != animation.animation) {
        engine::switchModelAnimation(animation, next, PoseFade);
    }
    const bool slashing = (state.flags & game::EntityState::Slashing) != 0 &&
                          std::hypot(state.velocityX, state.velocityY) > DashSpeed;
    BossModel &model = ecs.getComponent<BossModel>(entity);
    if (slashing && !model.slashing) {
        const float pan = (((state.x / game::GameWidth) * 2.0f) - 1.0f) * PanSpread;
        for (std::size_t sound = 0; sound < boss.slashSounds.size(); ++sound) {
            ecs.addComponent<engine::SoundEffect>(
                model.slashSources[sound], boss.slashSounds[sound].sound,
                audio::PlaySettings{.volume = boss.slashSounds[sound].volume, .pan = pan});
        }
    }
    model.slashing = slashing;
}

void BossLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity)
{
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    const render::Vector2 &centre = ecs.getComponent<BossModel>(entity).hullCentre;
    const ecs::Position &position = ecs.getComponent<ecs::Position>(entity);
    model.position = {position.x - (centre.x * model.scale), position.y - (centre.y * model.scale), 0.0f};
}

} // namespace rtype::client
