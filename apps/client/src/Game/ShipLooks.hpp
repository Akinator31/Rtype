#pragma once

#include "Game/EntityLook.hpp"
#include "ShipModel.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/Types.hpp"

#include <unordered_map>
#include <vector>

namespace rtype::client
{

/// A ship of the game drawn with a 3D model: it rolls around its length with its vertical speed, smoothly.
struct ShipAttitude {
    /// The centre of the hull of the model (ShipLook::hull), in model units: it is put on the entity.
    render::Vector2 hullCentre;
    /// The roll it turns towards, from the latest snapshot, and its roll now, in degrees: negative shows its
    /// belly to the camera, as it climbs, positive its top, as it dives.
    float targetRoll = 0.0f;
    float roll = 0.0f;
};

/// How the ships of a kind are drawn in 3D, instead of a box.
struct ShipLook {
    ShipModel model;
    /// The box around the hull of the model seen through the camera of the game (flames excluded), in model
    /// units, the model facing its way: the model is scaled and moved so that this box is the box of the ship
    /// in the game, its collider.
    render::Rectangle hull;
    /// The model is turned to face left: the Bydos fly towards the players.
    bool facesLeft = false;
    /// The meshes of the trails behind its flames, by index in the model file: never drawn, they stretch far
    /// behind the ship once it rolls its belly towards the camera.
    std::vector<int> trailMeshes;
};

/// The ShipLook of each kind of ship drawn in 3D, and the models they give to the entities of the game scene:
/// seen from the side and a little from above, rolling around their length, smoothly, as their ship climbs
/// or dives, with a little flame, whatever they do (docs/architecture/client-game.md, "Ships in 3D").
class ShipLooks
{
    std::unordered_map<game::EntityKind, ShipLook> _looks;

public:
    void setLook(game::EntityKind kind, const ShipLook &look);
    /// The ships of this kind are drawn in 3D.
    [[nodiscard]] bool hasLook(game::EntityKind kind) const;

    /// Gives an entity the model of its kind, with its ShipAttitude, placed on it.
    /// @param clock Seconds the session has been played: the time of the animations.
    void addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state, const EntityLook &look,
                  float clock) const;
    /// Turns the model of an entity towards the roll of the latest state of its ship, its animation in step
    /// with the session.
    void moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state, float clock) const;
    /// Rolls the model of an entity towards its roll for `elapsed` seconds, and puts its hull on the entity.
    static void placeModel(ecs::Ecs &ecs, ecs::entity_t entity, float elapsed);
};

} // namespace rtype::client
