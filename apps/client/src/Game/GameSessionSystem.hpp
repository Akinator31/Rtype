#pragma once

#include "Game/DamageLooks.hpp"
#include "Game/EntitySounds.hpp"
#include "Game/GameSession.hpp"
#include "Game/HealthBarLooks.hpp"
#include "Game/ImpactLooks.hpp"
#include "Game/ShipLooks.hpp"
#include "Game/ShotLooks.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rtype::client
{

/// An entity of the scene that draws an entity of the game.
struct Replicated {
    /// Its id in the snapshots.
    std::uint32_t networkId = 0;
    game::EntityKind kind = game::EntityKind::PlayerShip;
    /// The slot of the player it belongs to, or game::NoOwner.
    std::uint8_t owner = 0;
    /// Its hit points in the latest snapshot: when they drop, it was hit.
    std::uint8_t health = 0;
    /// The most hit points it had: the snapshots do not carry the health of a full entity, which appears
    /// with all of its own.
    std::uint8_t maxHealth = 0;
    /// Its health bar, once it was hit, if its kind shows one.
    std::optional<ecs::entity_t> healthBar;
};

/// A ship killed in the game while drawn, a Bydo or the ship of a player: for the events of the HUD.
struct Kill {
    game::EntityKind kind = game::EntityKind::PlayerShip;
    /// The slot of the player it belonged to, or game::NoOwner.
    std::uint8_t owner = 0;
    /// The slot of the player who killed a Bydo: the owner of the shot of a player killed with it nearest to
    /// it, as the game spends a shot that hits. None for a ship of a player, or a Bydo killed by no shot.
    std::optional<std::uint8_t> killer;
};

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
///    roll around their length, smoothly, as their ship climbs or dives; those given a ShotLook are models of
///    energy; the others are boxes. An entity killed in the game (a ship shot down, a shot that hit) leaves
///    the impact of its kind where it was last drawn, if its kind was given an ImpactLook. An entity whose
///    kind was given a DamageLook writes the damage it takes where it is hit: the hit points it lost since
///    the previous snapshot, or those it had left when killed. One whose kind was given a HealthBarLook shows
///    its health bar above it when it is hit, except the ship of the local player. The sounds are
///    engine::SoundEffect given to entities, which the engine::SoundSystem plays: a shot that appears has the
///    shot sound of its kind, the impact of an entity killed the destruction sound of its kind. Each ship
///    killed, a Bydo or the ship of a player, is told to the kill handler, if any, and each wave started to
///    the wave handler, if any.
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
    std::function<void(const Kill &)> _onKill;
    std::function<void(std::uint16_t)> _onWave;
    /// The entity of the scene of each entity of the game, by id.
    std::unordered_map<std::uint32_t, ecs::EntityRef> _entities;
    ShipLooks _shipLooks;
    ShotLooks _shotLooks;
    ImpactLooks _impactLooks;
    DamageLooks _damageLooks;
    HealthBarLooks _healthBarLooks;
    EntitySounds _shotSounds;
    EntitySounds _destructionSounds;
    /// The entities drawn that the game killed: they leave an impact when they go.
    std::unordered_set<std::uint32_t> _killed;
    std::vector<game::PlayerState> _players;
    std::uint32_t _tick = 0;
    /// Seconds the session has been played: the time of the animations of the ships.
    float _clock = 0.0f;

    void apply(const game::Snapshot &snapshot);
    /// Online, an entity is still drawn for a while after the game tells its destruction: the entities killed
    /// are remembered until they go.
    void rememberKilled(const std::vector<game::DestroyedEntity> &destroyed);
    /// Tells the kill handler about the ships among the entities killed.
    void reportKills(const std::vector<game::DestroyedEntity> &destroyed);
    /// The entities drawn among those destroyed by the game, not removed.
    [[nodiscard]] std::vector<ecs::entity_t>
    findKilled(const std::vector<game::DestroyedEntity> &destroyed) const;
    /// The owner of the shot of a player among the entities killed nearest to a Bydo killed, if any.
    [[nodiscard]] std::optional<std::uint8_t> findKiller(ecs::entity_t bydo,
                                                         const std::vector<ecs::entity_t> &killed) const;
    void createEntity(const game::EntityState &state);
    /// Destroys the entity of an entity of the game gone, which leaves its impact if it was killed.
    void removeEntity(std::uint32_t networkId, const ecs::EntityRef &entity);
    void moveEntity(ecs::entity_t entity, const game::EntityState &state);
    /// Writes the damage taken by an entity, at its position, if its kind shows it and it took any.
    void showDamage(ecs::entity_t entity, std::uint8_t damage);
    /// Shows the hit points left of an entity on its health bar, if its kind shows one: a hit creates the bar
    /// or shows it again, a heal fills the bar it has.
    void showHealth(ecs::entity_t entity, const game::EntityState &state, bool hit);
    /// The ship of the local player: its health bar would hide it.
    [[nodiscard]] bool isLocalShip(const game::EntityState &state) const;
    void placeShipModels(float elapsed);

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
    /// Draws the shots of this kind with a 3D model of energy from now on, instead of a box.
    void setShotLook(game::EntityKind kind, const ShotLook &look);
    /// Leaves an impact where the game kills an entity of this kind from now on.
    void setImpactLook(game::EntityKind kind, const ImpactLook &look);
    /// Writes the damage taken by the entities of this kind where they are hit from now on.
    void setDamageLook(game::EntityKind kind, const DamageLook &look);
    /// Shows the health bar of the entities of this kind above them when they are hit from now on.
    void setHealthBarLook(game::EntityKind kind, const HealthBarLook &look);
    /// Plays a sound when a shot of this kind appears from now on.
    void setShotSound(game::EntityKind kind, const EntitySound &sound);
    /// Plays a sound where the game kills an entity of this kind from now on, with its impact: a kind without
    /// an ImpactLook makes no sound.
    void setDestructionSound(game::EntityKind kind, const EntitySound &sound);
    /// Calls this for each ship killed in the game while drawn from now on: a Bydo or the ship of a player.
    void setKillHandler(std::function<void(const Kill &)> onKill);
    /// Calls this with the number of each wave started in the game from now on, from 1.
    void setWaveHandler(std::function<void(std::uint16_t)> onWave);

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
