#pragma once

#include "Game/GameSession.hpp"
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

/// How an entity of the game is drawn, for now: a coloured box of its size, the ships in the colour of
/// their player. Sprites can replace the boxes without changing the rest.
struct EntityLook {
    engine::Box box;
    int order = 0;
};

/// The look of an entity of the game.
[[nodiscard]] EntityLook lookOf(const game::EntityState &entity);

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
///    moved, and destroyed when it is no longer in the world.
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
    std::vector<game::PlayerState> _players;
    std::uint32_t _tick = 0;

    void apply(const game::Snapshot &snapshot);

public:
    GameSessionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, engine::Scenes &scenes,
                      engine::Controls &controls);

    /// Plays a session in a scene, open, instead of the previous one.
    /// @param onOver Called once when the session is over: a game over, or the game lost online.
    void start(std::unique_ptr<GameSession> session, engine::SceneId scene,
               std::function<void()> onOver = {});
    /// Stops the session. Its entities stay until their scene closes.
    void stop();

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
