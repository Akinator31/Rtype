#pragma once

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace rtype::client
{

/// An animation of the model of an item, by its index in the model and its length.
struct ItemAnimation {
    int index = 0;
    /// Seconds.
    float duration = 0.0f;
};

/// How the items of one kind of the configuration (game::ItemDefinition) are drawn: their 3D model, facing
/// the camera, fitted to the size of their hitbox, and the animations it has.
struct ItemLook {
    render::ModelId model;
    /// The box around the meshes of the model, in model units (render::ResourceManager::getModelBounds()).
    render::Box3 bounds;
    /// The size it is drawn at in the game, that of its collider, in game units.
    render::Vector2 size;
    /// Played in a loop while it lies in the game, if the model has it ("Idle").
    std::optional<ItemAnimation> idle;
    /// Played once where it is picked up, if the model has it ("Collect").
    std::optional<ItemAnimation> collect;
};

/// The model of an item drawn: it pops up when it appears, then floats while its Idle animation plays.
struct ItemModel {
    /// Its index in the configuration (game::EntityState::ship).
    std::uint8_t item = 0;
    /// Its scale once it has appeared.
    float scale = 1.0f;
    /// Seconds since it appeared.
    float age = 0.0f;
    /// Where the game has it.
    float x = 0.0f;
    float y = 0.0f;
    /// The centre of the box of the model, in model units: placed on the item.
    render::Vector3 centre;
};

/// The ItemLook of each kind of item, by its index in the configuration (game::EntityState::ship), and the
/// models they give to the entities of the game scene.
///
/// An item appears in `AppearDuration` seconds, growing from nothing with a small bounce, then floats up and
/// down while the Idle animation of its model loops. Picked up, it leaves its model playing its Collect
/// animation once where it was (an Impact, which the ImpactSystem plays and removes).
class ItemLooks
{
    std::unordered_map<std::uint8_t, ItemLook> _looks;

public:
    /// Seconds an item takes to appear: the time the game waits before it can be picked up
    /// (game::Rules::itemAppearTicks).
    static constexpr float AppearDuration = 0.5f;

    void setLook(std::uint8_t item, const ItemLook &look);
    /// The items of this index are drawn in 3D.
    [[nodiscard]] bool hasLook(std::uint8_t item) const;

    /// Gives an entity the model of its item, not grown yet, playing its Idle animation.
    void addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state) const;
    /// The latest place of the item in the game.
    static void moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state);
    /// Plays the appearance and the float of the model of an entity for `elapsed` seconds more.
    static void placeModel(ecs::Ecs &ecs, ecs::entity_t entity, float elapsed);

    /// The item drawn by `entity` was picked up: creates, in a scene, its model playing its Collect animation
    /// where it is drawn. Nothing if its model has none.
    std::optional<ecs::entity_t> createCollect(ecs::Ecs &ecs, engine::SceneId scene, ecs::entity_t entity) const;
};

} // namespace rtype::client
