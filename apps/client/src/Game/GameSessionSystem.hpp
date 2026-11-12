#pragma once

#include "Game/BossLooks.hpp"
#include "Game/DamageLooks.hpp"
#include "Game/EntitySounds.hpp"
#include "Game/GameSession.hpp"
#include "Game/HealthBarLooks.hpp"
#include "Game/ImpactLooks.hpp"
#include "Game/ItemLooks.hpp"
#include "Game/ShipLooks.hpp"
#include "Game/ShotLooks.hpp"
#include "Settings/AuraSettings.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Snapshot.hpp"
#include "rtype/render/ResourceManager.hpp"
#include "rtype/render/Types.hpp"

#include <chrono>
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
    /// The badge of the voice of the ship of another player, once the voice of the game is known.
    std::optional<ecs::entity_t> voiceBadge;
};

/// The badge above the ship of another player while it talks (Figma, Série 16, `Jeu · Badge de vaisseau`): a
/// microphone in a dark disc, right of its health bar.
struct VoiceBadgeLook {
    render::TextureId texture;
    /// Part of the texture, in texture pixels.
    render::Rectangle source;
    /// Its size, and where its centre is from the centre of the ship, in game units: y points down.
    float size = 30.0f;
    float x = 0.0f;
    float y = 0.0f;
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

/// The input of the local player, from the virtual gamepad of the engine: the directions, FaceDown (Space, A
/// on a gamepad) to fire, and LeftShoulder and RightShoulder (Q and E, A and E on AZERTY, the bumpers on a
/// gamepad) held to aim up and down.
[[nodiscard]] game::PlayerInput inputFrom(const engine::Controls &controls);

/// Plays the game session of the game scene (ADR 0015), each frame:
///
/// 1. the controls of the engine become the PlayerInput of the local player, an empty one when the scene does
///    not receive the input (a menu is over it) or while the input is locked (lockInputUntil());
/// 2. the session updates, paused while a menu is over the scene;
/// 3. each snapshot becomes entities of the scene: one per entity of the game, created when it appears,
///    moved, and destroyed when it is no longer in the world. The kinds given a ShipLook are 3D models, which
///    roll around their length, smoothly, as their ship climbs or dives, and recoil when a shot of the
///    players appears at their nose; the items given an ItemLook pop up, then turn and float; those given a
///    BossLook are standing
///    3D models, which play the animation of their stance; those given a ShotLook are models of energy; the
///    others are boxes. An entity killed in the game (a ship shot down, a shot that hit) leaves
///    the impact of its kind where it was last drawn, if its kind was given an ImpactLook. An entity whose
///    kind was given a DamageLook writes the damage it takes where it is hit: the hit points it lost since
///    the previous snapshot, or those it had left when killed. One whose kind was given a HealthBarLook shows
///    its health bar above it when it is hit, except the ship of the local player. The sounds are
///    engine::SoundEffect given to entities, which the engine::SoundSystem plays: a shot that appears has the
///    shot sound of its kind, the impact of an entity killed the destruction sound of its kind. Each ship
///    killed, a Bydo or the ship of a player, is told to the kill handler, if any, each wave started to
///    the wave handler, if any, each new stage of the rescue against the final boss to the rescue
///    handler, if any, and how the game ended to the outcome handler, if any. A ship the blade of the shogun
///    goes through during the rescue (game::Snapshot::struck) shows the strike impact and plays the strike
///    sound of its kind, if it was given them, and is told to the strike handler, if any.
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
    std::function<void(std::uint16_t, const std::string &)> _onWave;
    std::function<void(game::RescueStage)> _onRescue;
    std::function<void(game::GameOutcome)> _onOutcome;
    /// How the game ended, once a snapshot told it: a game ends once, though several snapshots may tell it.
    std::optional<game::GameOutcome> _outcome;
    std::function<void()> _onStrike;
    /// The local player holds the second weapon of its loadout: switched by SwitchWeaponControl, sent in
    /// every input (game::PlayerInput::SecondWeapon).
    bool _secondWeapon = false;
    /// The stage of the rescue against the final boss in the latest snapshot.
    game::RescueStage _rescue = game::RescueStage::None;
    /// The entity of the scene of each entity of the game, by id.
    std::unordered_map<std::uint32_t, ecs::EntityRef> _entities;
    ShipLooks _shipLooks;
    BossLooks _bossLooks;
    ShotLooks _shotLooks;
    ItemLooks _itemLooks;
    ImpactLooks _impactLooks;
    DamageLooks _damageLooks;
    HealthBarLooks _healthBarLooks;
    EntitySounds _shotSounds;
    EntitySounds _destructionSounds;
    /// The cut of the blade of the shogun on the ships it goes through during the rescue.
    ImpactLooks _strikeLooks;
    EntitySounds _strikeSounds;
    /// The entities drawn that the game killed: they leave an impact when they go.
    std::unordered_set<std::uint32_t> _killed;
    std::optional<VoiceBadgeLook> _voiceBadgeLook;
    std::vector<game::PlayerState> _players;
    /// Hit points of the ship of the local player in the latest snapshot, 0 while it is not there.
    std::uint8_t _localHealth = 0;
    /// The most hit points the ship of the local player had since it last appeared, 0 while it is not there.
    std::uint8_t _localMaxHealth = 0;
    /// A boss is in the latest snapshot.
    bool _boss = false;
    /// Where the ship of each player is in the latest snapshot, along the game (x), by slot.
    std::vector<std::pair<std::uint8_t, float>> _ships;
    /// Shots of the local player that appeared since the session started.
    std::uint32_t _localShots = 0;
    std::uint32_t _tick = 0;
    /// The local player has no control of its ship before this tick: during a cutscene.
    std::uint32_t _inputLockedUntil = 0;
    /// The last wave started in the session, 0 before the first.
    std::uint16_t _wave = 0;
    /// When the session started, for the time Discord counts since.
    std::chrono::system_clock::time_point _startedAt;
    /// Seconds the session has been played: the time of the animations of the ships.
    float _clock = 0.0f;
    /// The colours of the auras around the ships, chosen on the Settings page: none, no aura.
    const AuraColors *_auraColors = nullptr;

    void apply(const game::Snapshot &snapshot);
    /// Moves the entities to where the session draws them between two ticks: positions only.
    void place(const game::Snapshot &drawn);
    /// Online, an entity is still drawn for a while after the game tells its destruction: the entities killed
    /// are remembered until they go.
    void rememberKilled(const std::vector<game::DestroyedEntity> &destroyed);
    /// Shows the cut of the blade of the shogun on the ships it went through, and tells the strike handler.
    void showStrikes(const std::vector<std::uint32_t> &struck);
    /// Tells the kill handler about the ships among the entities killed.
    void reportKills(const std::vector<game::DestroyedEntity> &destroyed);
    /// The entities drawn among those destroyed by the game, not removed.
    [[nodiscard]] std::vector<ecs::entity_t>
    findKilled(const std::vector<game::DestroyedEntity> &destroyed) const;
    /// The owner of the shot of a player among the entities killed nearest to a Bydo killed, if any.
    [[nodiscard]] std::optional<std::uint8_t> findKiller(ecs::entity_t bydo,
                                                         const std::vector<ecs::entity_t> &killed) const;
    void createEntity(const game::EntityState &state);
    /// Casts the aura of its side on the 3D model of a ship, if it has one: the ship of the local player,
    /// those of the other players and of their allies, those of the Bydos. The bosses have none.
    void addAura(ecs::entity_t entity, const game::EntityState &state);
    /// Plays the recoil of the ship of the players that fired a shot, if it is drawn in 3D.
    void recoilShooter(const game::EntityState &shot);
    /// The ship of the players, or of their allies, drawn in 3D, nearest to a shot that appears, if it is
    /// near enough to have fired it: the snapshots do not tell which ship fired, and the allies fire the
    /// shots of a player.
    [[nodiscard]] std::optional<ecs::entity_t> findShooter(const game::EntityState &shot) const;
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
    /// Shows the badge of the voice above the ship of each other player who talks.
    void showVoiceBadges(float elapsed);
    /// Takes the other weapon of the loadout in hand when the local player presses SwitchWeaponControl, while
    /// it plays.
    void switchWeapon(bool playing);

public:
    GameSessionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, engine::Scenes &scenes,
                      engine::Controls &controls);

    /// Plays a session in a scene, open, instead of the previous one.
    /// @param onOver Called once when the session is over: a game over, or the game lost online.
    void start(std::unique_ptr<GameSession> session, engine::SceneId scene,
               std::function<void()> onOver = {});
    /// Stops the session. Its entities stay until their scene closes.
    void stop();
    /// The controls of the local player do nothing until this tick of the session played: its ship is left
    /// alone during a cutscene. Back to 0 when the session stops.
    void lockInputUntil(std::uint32_t tick) { _inputLockedUntil = tick; }
    /// Draws the ships of this kind with a 3D model from now on, instead of a box.
    void setShipLook(game::EntityKind kind, const ShipLook &look);
    /// Draws the ships of this kind with these 3D models from now on, each ship with one of them, picked by
    /// its id.
    void setShipLooks(game::EntityKind kind, std::vector<ShipLook> looks);
    /// Draws the entities that fly this ship of the configuration (game::EntityState::ship) with this 3D
    /// model from now on, at this size, that of its collider, instead of the look of their kind.
    void setShipLook(std::uint8_t ship, const ShipLook &look, render::Vector2 size);
    /// Draws the ship of the local player with this 3D model from now on, instead of the look of its kind.
    void setLocalShipLook(const ShipLook &look);
    /// Draws the bosses of this kind with a 3D model from now on, instead of a box.
    void setBossLook(game::EntityKind kind, const BossLook &look);
    /// Draws the shots of this kind with a 3D model of energy from now on, instead of a box.
    void setShotLook(game::EntityKind kind, const ShotLook &look);
    /// Draws the shots of the players fired with the projectile of this index of the configuration
    /// (game::EntityState::ship) with this look, at this size, from now on: each weapon its own model.
    void setProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size);
    /// The same for the shots of the Bydos.
    void setBydoProjectileLook(std::uint8_t projectile, const ShotLook &look, render::Vector2 size);
    /// Draws the items of this index of the configuration (game::EntityState::ship) with this animated 3D
    /// model from now on, instead of a box.
    void setItemLook(std::uint8_t item, const ItemLook &look);
    /// Leaves an impact where the game kills an entity of this kind from now on.
    void setImpactLook(game::EntityKind kind, const ImpactLook &look);
    /// Writes the damage taken by the entities of this kind where they are hit from now on.
    void setDamageLook(game::EntityKind kind, const DamageLook &look);
    /// Shows the health bar of the entities of this kind above them when they are hit from now on.
    void setHealthBarLook(game::EntityKind kind, const HealthBarLook &look);
    /// Shows this badge above the ship of each other player while it talks, from now on.
    void setVoiceBadgeLook(const VoiceBadgeLook &look) { _voiceBadgeLook = look; }
    /// Plays a sound when a shot of this kind appears from now on.
    void setShotSound(game::EntityKind kind, const EntitySound &sound);
    /// Plays a sound where the game kills an entity of this kind from now on, with its impact: a kind without
    /// an ImpactLook makes no sound.
    void setDestructionSound(game::EntityKind kind, const EntitySound &sound);
    /// Leaves this impact on a ship of this kind that the blade of the shogun goes through from now on,
    /// during the rescue against the final boss.
    void setStrikeLook(game::EntityKind kind, const ImpactLook &look);
    /// Plays a sound where the blade of the shogun goes through a ship of this kind from now on, with its
    /// strike impact: a kind without a strike look makes no sound.
    void setStrikeSound(game::EntityKind kind, const EntitySound &sound);
    /// Surrounds the ships with an aura in these colours from now on, read when a ship appears: the colours
    /// must outlive the system. None, the default, draws no aura.
    void setAuraColors(const AuraColors *colors);
    /// Calls this for each ship drawn that the blade of the shogun goes through from now on.
    void setStrikeHandler(std::function<void()> onStrike);

    /// Calls this for each ship killed in the game while drawn from now on: a Bydo or the ship of a player.
    void setKillHandler(std::function<void(const Kill &)> onKill);
    /// Calls this with the number of each wave started in the game from now on, from 1, and its music (empty
    /// if it has none).
    void setWaveHandler(std::function<void(std::uint16_t, const std::string &)> onWave);
    /// Calls this with each new stage of the rescue against the final boss from now on (offline only).
    void setRescueHandler(std::function<void(game::RescueStage)> onRescue);
    /// Calls this once per session from now on, with how the game ended, when a snapshot tells it.
    void setOutcomeHandler(std::function<void(game::GameOutcome)> onOutcome);

    void update(float elapsed) override;

    /// A session is played.
    [[nodiscard]] bool running() const { return _session != nullptr; }
    /// The session played, if any.
    [[nodiscard]] GameSession *session() const { return _session.get(); }
    /// The scene the session is played in, if any.
    [[nodiscard]] std::optional<engine::SceneId> scene() const { return _scene; }
    /// The players of the latest snapshot, by slot: for the HUD.
    [[nodiscard]] const std::vector<game::PlayerState> &players() const { return _players; }
    /// Hit points of the ship of the local player in the latest snapshot: for the HUD.
    [[nodiscard]] std::uint8_t localHealth() const { return _localHealth; }
    /// The local player holds the second weapon of its loadout (GameSession::loadout()): for the HUD.
    [[nodiscard]] bool holdsSecondWeapon() const { return _secondWeapon; }
    /// The most hit points the ship of the local player had since it appeared, its full health: 0 while it is
    /// not there. The cockpit of a VR headset warns when the ship is low on life.
    [[nodiscard]] std::uint8_t localMaxHealth() const { return _localMaxHealth; }
    /// A boss is in the game, in the latest snapshot.
    [[nodiscard]] bool bossIsThere() const { return _boss; }
    /// Where the ship of each player is along the game (x, in game units) in the latest snapshot, by slot,
    /// the local player included: the voices of the others are heard from their side in a VR headset.
    [[nodiscard]] const std::vector<std::pair<std::uint8_t, float>> &ships() const { return _ships; }
    /// How many shots of the local player appeared since the session started: the controllers of a VR
    /// headset tick at each one.
    [[nodiscard]] std::uint32_t localShots() const { return _localShots; }
    /// The tick of the latest snapshot.
    [[nodiscard]] std::uint32_t tick() const { return _tick; }
    /// The last wave started in the session played, from 1; 0 before the first.
    [[nodiscard]] std::uint16_t wave() const { return _wave; }
    /// When the session played started.
    [[nodiscard]] std::chrono::system_clock::time_point startedAt() const { return _startedAt; }
    /// The stage of the rescue against the final boss in the latest snapshot.
    [[nodiscard]] game::RescueStage rescue() const { return _rescue; }
    /// How the game played ended, once a snapshot told it.
    [[nodiscard]] std::optional<game::GameOutcome> outcome() const { return _outcome; }
    /// The entity of the scene that draws an entity of the game, if it is drawn.
    [[nodiscard]] std::optional<ecs::entity_t> entityOf(std::uint32_t networkId) const;
    /// Entities of the game drawn.
    [[nodiscard]] std::size_t entityCount() const { return _entities.size(); }
};

} // namespace rtype::client
