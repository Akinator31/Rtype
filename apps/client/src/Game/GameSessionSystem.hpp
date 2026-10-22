#pragma once

#include "Game/GameSession.hpp"
#include "ShipModel.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace rtype::client
{

/// An entity of the scene that draws an entity of the game.
struct Replicated {
    /// Its id in the snapshots.
    std::uint32_t networkId = 0;
};

/// A ship of the game drawn with a 3D model: it rolls around its length with its vertical speed, smoothly.
struct ShipAttitude {
    /// The centre of the hull of the model (ShipLook::hull), in model units: it is put on the entity.
    render::Vector2 hullCentre;
    /// The roll it turns towards, from the latest snapshot, and its roll now, in degrees: negative shows its
    /// belly to the camera, as it climbs, positive its top, as it dives.
    float targetRoll = 0.0f;
    float roll = 0.0f;
};

/// How an entity of the game is drawn without a model: a coloured box of its size, the ships in the colour of
/// their player. The size is that of its collider in the game.
struct EntityLook {
    engine::Box box;
    int order = 0;
};

/// The look of an entity of the game.
[[nodiscard]] EntityLook lookOf(const game::EntityState &entity);

/// How the ships of a kind are drawn in 3D, instead of a box.
struct ShipLook {
    ShipModel model;
    /// The box around the hull of the model seen through the camera of the game (flames excluded), in model
    /// units, the model facing its way: the model is scaled and moved so that this box is the box of the ship
    /// in the game, its collider.
    render::Rectangle hull;
    /// The model is turned to face left: the Bydos fly towards the players.
    bool facesLeft = false;
    /// The meshes of its flames and trails, by index in the model file: hidden while the ship does not fly
    /// forward.
    std::vector<int> flameMeshes;
};

/// The camera of the game scene: an orthographic camera that sees the plane z = 0 as the screen sees the
/// game, a world unit for a game unit, y pointing down. A model at the position of an entity of the game is
/// drawn where its box would be, and keeps its size anywhere on the screen.
/// @param visibleArea The part of the game that the screen shows (render::Renderer::visibleArea()).
[[nodiscard]] render::Camera computeGameView(render::Rectangle visibleArea);

/// The colour of a player: blue, red, green, yellow for the slots 0 to 3.
[[nodiscard]] render::Color playerColor(std::uint8_t slot);

/// The input of the local player, from the virtual gamepad of the engine: the directions, and FaceDown
/// (Space, A on a gamepad) to fire.
[[nodiscard]] game::PlayerInput inputFrom(const engine::Controls &controls);

/// Plays the game session of the game scene (ADR 0015), each frame:
///
/// 1. the controls of the engine become the PlayerInput of the local player, an empty one when the scene does
///    not receive the input (a menu is over it);
/// 2. the session updates, paused while a menu is over the scene;
/// 3. each snapshot becomes entities of the scene: one per entity of the game, created when it appears,
///    moved, and destroyed when it is no longer in the world. The kinds given a ShipLook are 3D models, which
///    roll around their length, smoothly, as their ship climbs or dives; the others are boxes.
///
/// @code
/// gameSessions.start(std::make_unique<LocalSession>(), scene); // when the game scene opens
/// @endcode
///
/// The session stops by itself when its scene closes: the engine destroys the entities.
class GameSessionSystem : public ecs::System
{
    ecs::Ecs &_ecs;
    engine::Scenes &_scenes;
    engine::Controls &_controls;
    std::unique_ptr<GameSession> _session;
    std::optional<engine::SceneId> _scene;
    std::function<void()> _onOver;
    /// The entity of the scene of each entity of the game, by id.
    std::unordered_map<std::uint32_t, ecs::EntityRef> _entities;
    std::unordered_map<game::EntityKind, ShipLook> _shipLooks;
    std::vector<game::PlayerState> _players;
    std::uint32_t _tick = 0;
    /// Seconds the session has been played: the time of the animations of the ships.
    float _clock = 0.0f;

    void apply(const game::Snapshot &snapshot);
    void createEntity(const game::EntityState &state);
    void addModel(ecs::entity_t entity, const game::EntityState &state, const EntityLook &look,
                  const ShipLook &ship);
    void moveModel(ecs::entity_t entity, const game::EntityState &state);
    /// Rolls the model of a ship towards its roll for `elapsed` seconds, and puts its hull on the entity.
    void placeModel(ecs::entity_t entity, float elapsed);
    void placeModels(float elapsed);

public:
    GameSessionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, engine::Scenes &scenes,
                      engine::Controls &controls);

    /// Plays a session in a scene, open, instead of the previous one.
    /// @param onOver Called once when the session is over: a game over, or the game lost online.
    void start(std::unique_ptr<GameSession> session, engine::SceneId scene,
               std::function<void()> onOver = {});
    /// Stops the session. Its entities stay until their scene closes.
    void stop();
    /// Draws the ships of this kind with a 3D model from now on, instead of a box.
    void setShipLook(game::EntityKind kind, const ShipLook &look);

    void update(float elapsed) override;

    /// A session is played.
    [[nodiscard]] bool running() const { return _session != nullptr; }
    /// The session played, if any.
    [[nodiscard]] GameSession *session() const { return _session.get(); }
    /// The players of the latest snapshot, by slot: for the HUD.
    [[nodiscard]] const std::vector<game::PlayerState> &players() const { return _players; }
    /// The tick of the latest snapshot.
    [[nodiscard]] std::uint32_t tick() const { return _tick; }
    /// The entity of the scene that draws an entity of the game, if it is drawn.
    [[nodiscard]] std::optional<ecs::entity_t> entityOf(std::uint32_t networkId) const;
    /// Entities of the game drawn.
    [[nodiscard]] std::size_t entityCount() const { return _entities.size(); }
};

} // namespace rtype::client
