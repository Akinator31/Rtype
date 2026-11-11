#pragma once

#include "Game/EntityLook.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/Energy.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstdint>
#include <unordered_map>

namespace rtype::client
{

/// How the shots of a kind are drawn in 3D, instead of a box: a model of energy.
struct ShotLook {
    render::ModelId model;
    /// The box around the core of the model seen through the camera of the game (its trail and its rings
    /// excluded), in model units, the model facing its way: the model is scaled and moved so that this box
    /// is the box of the shot, its collider.
    render::Rectangle core;
    /// The model is turned to face left: the shots of the Bydos.
    bool facesLeft = false;
    render::Energy energy;
};

/// The ShotLook of each kind of shot drawn in 3D, and the models of energy they give to the entities of the
/// game scene (docs/architecture/client-game.md, "Shots in 3D"). A shot, of a player or of a Bydo, whose
/// projectile has a look of its own (game::EntityState::ship) is drawn with it, at the size of the collider
/// of the projectile: each weapon fires its own model.
class ShotLooks
{
    struct ProjectileLook {
        ShotLook look;
        /// The size of the collider of the projectile, in game units.
        render::Vector2 size;
    };

    std::unordered_map<game::EntityKind, ShotLook> _looks;
    std::unordered_map<std::uint8_t, ProjectileLook> _projectiles;
    std::unordered_map<std::uint8_t, ProjectileLook> _bydoProjectiles;

    /// The look of the projectile of the shot, if it has one.
    [[nodiscard]] const ProjectileLook *findProjectile(const game::EntityState &state) const;
    [[nodiscard]] const ShotLook &lookOf(const game::EntityState &state) const;

public:
    void setLook(game::EntityKind kind, const ShotLook &look);
    /// The shots of the players with the projectile of this index of the configuration are drawn with this
    /// look, at this size, instead of the look of their kind.
    void setProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size);
    /// The same for the shots of the Bydos, lit in their own colours.
    void setBydoProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size);
    /// The shots of this kind are drawn in 3D.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;
    /// This shot is drawn in 3D: its kind or its projectile has a look.
    [[nodiscard]] bool hasLook(const game::EntityState &state) const;

    /// Gives an entity the model of its kind, placed on it.
    void addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                  const EntityLook &look) const;
    /// Puts the core of the model of an entity on the latest state of its shot, turned the way it flies: a
    /// shot aimed at a player flies askew.
    void placeModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const;
};

} // namespace rtype::client
