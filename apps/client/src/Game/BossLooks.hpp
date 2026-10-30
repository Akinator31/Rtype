#pragma once

#include "Game/EntityLook.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <string>
#include <unordered_map>

namespace rtype::client
{

/// The animations of a boss model, by index (render::ResourceManager::getModelAnimation()): it stands, flies,
/// strikes with its blade, guards, aims its gun.
struct BossAnimations {
    int idle = 0;
    int fly = 0;
    int slash = 0;
    int guard = 0;
    int shoot = 0;
};

/// How the bosses of a kind are drawn: a standing figure, which plays the animation of what it does.
struct BossLook {
    render::ModelId model;
    BossAnimations animations;
    /// The box around its body seen through the camera of the game (weapons excluded), in model units, turned
    /// by `spin`: the model is scaled and moved so that this box is the box of the boss in the game.
    render::Rectangle hull;
    /// How much the model is turned around its vertical axis, in degrees: to face left, towards the players.
    float spin = 0.0f;
};

/// The model of a boss on its entity: the centre of its hull, which is put on the entity.
struct BossModel {
    render::Vector2 hullCentre;
};

/// Loads a boss model and finds its animations: Idle, Fly, Slash, Guard and Shoot.
/// @throws std::runtime_error if the model cannot be loaded, std::out_of_range if it misses an animation.
[[nodiscard]] BossLook loadBossLook(render::ResourceManager &resources, const std::string &path,
                                    render::Rectangle hull, float spin);

/// The BossLook of each kind of boss, and the models they give to the entities of the game scene. A boss
/// plays the animation of the stance of its latest state (game::EntityState::Guarding, Slashing, Shooting),
/// Fly while it moves without one, Idle otherwise, fading from one to the next.
class BossLooks
{
    std::unordered_map<game::EntityKind, BossLook> _looks;

public:
    void setLook(game::EntityKind kind, const BossLook &look);
    /// The bosses of this kind are drawn in 3D.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Gives an entity the model of its kind, with its BossModel, placed on it.
    void addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                  const EntityLook &look) const;
    /// Plays the animation of the latest state of its boss on the model of an entity.
    void moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const;
    /// Puts the hull of the model of an entity on the entity.
    static void placeModel(ecs::Ecs &ecs, ecs::entity_t entity);
};

} // namespace rtype::client
