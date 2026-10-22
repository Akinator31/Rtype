#pragma once

#include "Game/EntityLook.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/Energy.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

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
/// game scene (docs/architecture/client-game.md, "Shots in 3D").
class ShotLooks
{
    std::unordered_map<game::EntityKind, ShotLook> _looks;

public:
    void setLook(game::EntityKind kind, const ShotLook &look);
    /// The shots of this kind are drawn in 3D.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Gives an entity the model of its kind, placed on it.
    void addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                  const EntityLook &look) const;
    /// Puts the core of the model of an entity on the latest state of its shot, turned the way it flies: a
    /// shot aimed at a player flies askew.
    void placeModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const;
};

} // namespace rtype::client
