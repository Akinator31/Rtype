#include "Game/ShotLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/engine/Component/Drawing.hpp"

#include <cmath>
#include <vector>

namespace rtype::client
{

namespace
{

/// How far the shot turns, in radians, from the way its model faces to the way it flies: 0 for a shot fired
/// straight ahead, positive towards the bottom of the game area.
float computeHeading(const ShotLook &shot, const game::EntityState &state)
{
    const float facing = shot.facesLeft ? -1.0f : 1.0f;
    if (state.velocityX == 0.0f && state.velocityY == 0.0f) {
        return 0.0f;
    }
    return std::atan2(facing * state.velocityY, facing * state.velocityX);
}

/// The axis of the half turn that flips the model around its length, as for a shot fired straight ahead, and
/// turns it by `heading` in the plane of the game: an axis at half the heading.
render::Vector3 computeFlipAxis(float heading)
{
    return {std::cos(heading / 2.0f), std::sin(heading / 2.0f), 0.0f};
}

/// A vector of the plane of the game turned by `angle` radians, towards +y.
render::Vector2 turnVector(render::Vector2 vector, float angle)
{
    return {(vector.x * std::cos(angle)) - (vector.y * std::sin(angle)),
            (vector.x * std::sin(angle)) + (vector.y * std::cos(angle))};
}

} // namespace

void ShotLooks::setLook(game::EntityKind kind, const ShotLook &look)
{
    _looks.insert_or_assign(kind, look);
}

void ShotLooks::setProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size)
{
    _projectiles.insert_or_assign(projectile, ProjectileLook{look, size});
}

void ShotLooks::setBydoProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size)
{
    _bydoProjectiles.insert_or_assign(projectile, ProjectileLook{look, size});
}

bool ShotLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

const ShotLooks::ProjectileLook *ShotLooks::findProjectile(const game::EntityState &state) const
{
    const bool bydo =
        state.kind == game::EntityKind::BydoShot || state.kind == game::EntityKind::BydoPlasmaShot;
    if (state.kind != game::EntityKind::PlayerShot && !bydo) {
        return nullptr;
    }
    const auto &projectiles = bydo ? _bydoProjectiles : _projectiles;
    const auto found = projectiles.find(state.ship);
    return found == projectiles.end() ? nullptr : &found->second;
}

bool ShotLooks::hasLook(const game::EntityState &state) const
{
    return findProjectile(state) != nullptr || hasLook(state.kind);
}

const ShotLook &ShotLooks::lookOf(const game::EntityState &state) const
{
    const ProjectileLook *projectile = findProjectile(state);
    return projectile != nullptr ? projectile->look : _looks.at(state.kind);
}

void ShotLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                         const EntityLook &look) const
{
    const ProjectileLook *projectile = findProjectile(state);
    const ShotLook &shot = lookOf(state);
    const float width = projectile != nullptr ? projectile->size.x : look.box.width;
    ecs.addComponent<engine::Model>(entity, shot.model, render::Vector3{}, LengthAxis, HalfTurn,
                                    width / shot.core.width, render::colors::White,
                                    shot.facesLeft ? HalfTurn : 0.0f, std::vector<int>{}, shot.energy);
    placeModel(ecs, entity, state);
}

void ShotLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const
{
    const ShotLook &shot = lookOf(state);
    const float heading = computeHeading(shot, state);
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    model.rotationAxis = computeFlipAxis(heading);
    const render::Rectangle &core = shot.core;
    const render::Vector2 centre{core.x + (core.width / 2.0f), core.y + (core.height / 2.0f)};
    const render::Vector2 offset = turnVector({centre.x * model.scale, centre.y * model.scale}, heading);
    model.position = {state.x - offset.x, state.y - offset.y, 0.0f};
}

} // namespace rtype::client
