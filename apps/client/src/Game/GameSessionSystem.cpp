#include "Game/GameSessionSystem.hpp"

#include "Game/EntityLook.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Control/Control.hpp"
#include "rtype/game/Constants.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace rtype::client
{

namespace
{

/// The kinds of the Bydos, shots excluded: 0x10 to 0x3F in the protocol.
bool isBydoShip(game::EntityKind kind)
{
    const auto value = static_cast<std::uint8_t>(kind);
    return value >= 0x10 && value < 0x40;
}

bool isPlayerShot(game::EntityKind kind)
{
    return kind == game::EntityKind::PlayerShot || kind == game::EntityKind::PlayerChargedShot;
}

float distanceBetween(const ecs::Position &first, const ecs::Position &second)
{
    return std::hypot(first.x - second.x, first.y - second.y);
}

/// The ships on the side of the players: theirs, and their allies during the rescue.
bool isPlayerSideShip(game::EntityKind kind)
{
    return kind == game::EntityKind::PlayerShip || kind == game::EntityKind::AllyShip;
}

/// A shot appears at the nose of the ship that fired it, half a ship ahead of its centre, and has flown for a
/// snapshot or so when it is drawn: no farther than this from its centre.
const float ShooterReach = 2.0f * game::Rules{}.shipHalfWidth;

} // namespace

game::PlayerInput inputFrom(const engine::Controls &controls)
{
    game::PlayerInput input;
    input.set(game::PlayerInput::Up, controls.isDown(engine::Control::Up));
    input.set(game::PlayerInput::Down, controls.isDown(engine::Control::Down));
    input.set(game::PlayerInput::Left, controls.isDown(engine::Control::Left));
    input.set(game::PlayerInput::Right, controls.isDown(engine::Control::Right));
    input.set(game::PlayerInput::Fire, controls.isDown(engine::Control::FaceDown));
    input.set(game::PlayerInput::AimUp, controls.isDown(engine::Control::LeftShoulder));
    input.set(game::PlayerInput::AimDown, controls.isDown(engine::Control::RightShoulder));
    return input;
}

GameSessionSystem::GameSessionSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, engine::Scenes &scenes,
                                     engine::Controls &controls)
    : _ecs(ecs), _scenes(scenes), _controls(controls)
{
    use<Replicated>(caches);
    use<ecs::Position>(caches);
    use<engine::Box>(caches);
    use<engine::Model>(caches);
    use<engine::ModelAnimation>(caches);
    use<ShipAttitude>(caches);
    use<BossModel>(caches);
    use<engine::Drawable>(caches);
    use<engine::SceneMember>(caches);
    use<Impact>(caches);
    use<engine::Text>(caches);
    use<engine::Opacity>(caches);
    use<engine::Scale>(caches);
    use<engine::Tween>(caches);
    use<DamageNumber>(caches);
    use<ecs::Parent>(caches);
    use<ecs::LocalPosition>(caches);
    use<engine::Gauge>(caches);
    use<HealthBar>(caches);
    use<engine::SoundEffect>(caches);
}

void GameSessionSystem::start(std::unique_ptr<GameSession> session, engine::SceneId scene,
                              std::function<void()> onOver)
{
    stop();
    _session = std::move(session);
    _scene = scene;
    _onOver = std::move(onOver);
}

void GameSessionSystem::stop()
{
    _session.reset();
    _scene.reset();
    _onOver = nullptr;
    _entities.clear();
    _killed.clear();
    _players.clear();
    _tick = 0;
    _rescue = game::RescueStage::None;
    _clock = 0.0f;
}

void GameSessionSystem::setShipLook(game::EntityKind kind, const ShipLook &look)
{
    _shipLooks.setLook(kind, look);
}

void GameSessionSystem::setShipLooks(game::EntityKind kind, std::vector<ShipLook> looks)
{
    _shipLooks.setLooks(kind, std::move(looks));
}

void GameSessionSystem::setLocalShipLook(const ShipLook &look)
{
    _shipLooks.setLocalLook(look);
}

void GameSessionSystem::setBossLook(game::EntityKind kind, const BossLook &look)
{
    _bossLooks.setLook(kind, look);
}

void GameSessionSystem::setShotLook(game::EntityKind kind, const ShotLook &look)
{
    _shotLooks.setLook(kind, look);
}

void GameSessionSystem::setImpactLook(game::EntityKind kind, const ImpactLook &look)
{
    _impactLooks.setLook(kind, look);
}

void GameSessionSystem::setDamageLook(game::EntityKind kind, const DamageLook &look)
{
    _damageLooks.setLook(kind, look);
}

void GameSessionSystem::setHealthBarLook(game::EntityKind kind, const HealthBarLook &look)
{
    _healthBarLooks.setLook(kind, look);
}

void GameSessionSystem::setShotSound(game::EntityKind kind, const EntitySound &sound)
{
    _shotSounds.setSound(kind, sound);
}

void GameSessionSystem::setDestructionSound(game::EntityKind kind, const EntitySound &sound)
{
    _destructionSounds.setSound(kind, sound);
}

void GameSessionSystem::setStrikeLook(game::EntityKind kind, const ImpactLook &look)
{
    _strikeLooks.setLook(kind, look);
}

void GameSessionSystem::setStrikeSound(game::EntityKind kind, const EntitySound &sound)
{
    _strikeSounds.setSound(kind, sound);
}

void GameSessionSystem::setStrikeHandler(std::function<void()> onStrike)
{
    _onStrike = std::move(onStrike);
}

void GameSessionSystem::setKillHandler(std::function<void(const Kill &)> onKill)
{
    _onKill = std::move(onKill);
}

void GameSessionSystem::setWaveHandler(std::function<void(std::uint16_t, const std::string &)> onWave)
{
    _onWave = std::move(onWave);
}

void GameSessionSystem::setRescueHandler(std::function<void(game::RescueStage)> onRescue)
{
    _onRescue = std::move(onRescue);
}

std::optional<ecs::entity_t> GameSessionSystem::entityOf(std::uint32_t networkId) const
{
    const auto found = _entities.find(networkId);
    if (found == _entities.end() || !_ecs.isAlive(found->second)) {
        return std::nullopt;
    }
    return found->second.id;
}

void GameSessionSystem::update(float elapsed)
{
    if (!_session) {
        return;
    }
    if (!_scene || !_scenes.isOpen(*_scene)) {
        // The scene closed: the engine destroyed its entities
        stop();
        return;
    }
    if (!_scenes.updates(*_scene)) {
        return;
    }
    const bool playing = _scenes.receivesInput(*_scene);
    _session->setPaused(!playing);
    _session->setInput(playing ? inputFrom(_controls) : game::PlayerInput{});
    _session->update(elapsed);
    _clock += elapsed;
    for (const game::Snapshot &snapshot : _session->takeSnapshots()) {
        apply(snapshot);
    }
    placeShipModels(elapsed);
    if (_session->isOver() && _onOver) {
        // Once: it usually switches scenes, at the next update
        const std::function<void()> onOver = std::exchange(_onOver, nullptr);
        onOver();
    }
}

void GameSessionSystem::apply(const game::Snapshot &snapshot)
{
    _tick = snapshot.tick;
    _players = snapshot.players;

    std::unordered_set<std::uint32_t> present;
    present.reserve(snapshot.entities.size());
    for (const game::EntityState &state : snapshot.entities) {
        present.insert(state.id);
        const auto found = _entities.find(state.id);
        if (found == _entities.end() || !_ecs.isAlive(found->second)) {
            createEntity(state);
            continue;
        }
        moveEntity(found->second.id, state);
    }
    showStrikes(snapshot.struck);
    rememberKilled(snapshot.destroyed);
    reportKills(snapshot.destroyed);
    if (snapshot.waveStarted && _onWave) {
        _onWave(*snapshot.waveStarted, snapshot.waveMusic);
    }
    if (snapshot.rescue != std::exchange(_rescue, snapshot.rescue) && _onRescue) {
        _onRescue(snapshot.rescue);
    }

    // Each snapshot holds the whole world: what is not in it any more is gone
    for (auto entry = _entities.begin(); entry != _entities.end();) {
        if (present.contains(entry->first)) {
            ++entry;
            continue;
        }
        removeEntity(entry->first, entry->second);
        entry = _entities.erase(entry);
    }
}

void GameSessionSystem::showStrikes(const std::vector<std::uint32_t> &struck)
{
    for (const std::uint32_t networkId : struck) {
        const std::optional<ecs::entity_t> ship = entityOf(networkId);
        if (!ship) {
            continue;
        }
        const game::EntityKind kind = _ecs.getComponent<Replicated>(*ship).kind;
        if (_strikeLooks.hasLook(kind)) {
            const ecs::Position position = _ecs.getComponent<ecs::Position>(*ship);
            const ecs::entity_t impact = _strikeLooks.createImpact(_ecs, *_scene, kind, position);
            _strikeSounds.addSound(_ecs, impact, kind, position.x);
        }
        if (_onStrike) {
            _onStrike();
        }
    }
}

void GameSessionSystem::rememberKilled(const std::vector<game::DestroyedEntity> &destroyed)
{
    for (const game::DestroyedEntity &entity : destroyed) {
        if (entity.cause == game::DestroyCause::Killed && _entities.contains(entity.id)) {
            _killed.insert(entity.id);
        }
    }
}

void GameSessionSystem::reportKills(const std::vector<game::DestroyedEntity> &destroyed)
{
    if (!_onKill) {
        return;
    }
    const std::vector<ecs::entity_t> killed = findKilled(destroyed);
    for (const ecs::entity_t entity : killed) {
        const Replicated &replicated = _ecs.getComponent<Replicated>(entity);
        if (replicated.kind == game::EntityKind::PlayerShip) {
            _onKill({replicated.kind, replicated.owner, std::nullopt});
        } else if (isBydoShip(replicated.kind)) {
            _onKill({replicated.kind, replicated.owner, findKiller(entity, killed)});
        }
    }
}

std::vector<ecs::entity_t>
GameSessionSystem::findKilled(const std::vector<game::DestroyedEntity> &destroyed) const
{
    std::vector<ecs::entity_t> killed;
    for (const game::DestroyedEntity &entity : destroyed) {
        const auto found = _entities.find(entity.id);
        if (entity.cause == game::DestroyCause::Killed && found != _entities.end() &&
            _ecs.isAlive(found->second)) {
            killed.push_back(found->second.id);
        }
    }
    return killed;
}

std::optional<std::uint8_t> GameSessionSystem::findKiller(ecs::entity_t bydo,
                                                          const std::vector<ecs::entity_t> &killed) const
{
    const ecs::Position &position = _ecs.getComponent<ecs::Position>(bydo);
    std::optional<std::uint8_t> killer;
    float nearest = std::numeric_limits<float>::max();
    for (const ecs::entity_t entity : killed) {
        const Replicated &shot = _ecs.getComponent<Replicated>(entity);
        const float distance = distanceBetween(position, _ecs.getComponent<ecs::Position>(entity));
        if (isPlayerShot(shot.kind) && shot.owner < game::MaxPlayers && distance < nearest) {
            nearest = distance;
            killer = shot.owner;
        }
    }
    return killer;
}

void GameSessionSystem::createEntity(const game::EntityState &state)
{
    const EntityLook look = lookOf(state);
    const ecs::entity_t entity = _ecs.createEntity();
    _ecs.addComponent<engine::SceneMember>(entity, *_scene);
    _ecs.addComponent<Replicated>(entity, state.id, state.kind, state.owner, state.health, state.health);
    _ecs.addComponent<ecs::Position>(entity, state.x, state.y);
    _ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, look.order);
    if (_shipLooks.hasLook(state.kind)) {
        _shipLooks.addModel(_ecs, entity, state, look, _clock, isLocalShip(state));
    } else if (_bossLooks.hasLook(state.kind)) {
        _bossLooks.addModel(_ecs, entity, state, look);
    } else if (_shotLooks.hasLook(state.kind)) {
        _shotLooks.addModel(_ecs, entity, state, look);
    } else {
        _ecs.addComponent<engine::Box>(entity, look.box);
    }
    if (const std::optional<physics::Collider> collider = _session->findCollider(state)) {
        _ecs.addComponent<physics::Collider>(entity, *collider);
    }
    _shotSounds.addSound(_ecs, entity, state.kind, state.x);
    _entities.insert_or_assign(state.id, _ecs.ref(entity));
    if (isPlayerShot(state.kind)) {
        recoilShooter(state);
    }
}

void GameSessionSystem::recoilShooter(const game::EntityState &shot)
{
    if (const std::optional<ecs::entity_t> shooter = findShooter(shot)) {
        ShipLooks::fireModel(_ecs, *shooter);
    }
}

std::optional<ecs::entity_t> GameSessionSystem::findShooter(const game::EntityState &shot) const
{
    const ecs::Position position{shot.x, shot.y};
    std::optional<ecs::entity_t> shooter;
    float nearest = ShooterReach;
    for (const auto &[networkId, entity] : _entities) {
        if (!_ecs.isAlive(entity) || !_ecs.hasComponent<ShipAttitude>(entity.id) ||
            !isPlayerSideShip(_ecs.getComponent<Replicated>(entity.id).kind)) {
            continue;
        }
        const float distance = distanceBetween(position, _ecs.getComponent<ecs::Position>(entity.id));
        if (distance <= nearest) {
            nearest = distance;
            shooter = entity.id;
        }
    }
    return shooter;
}

void GameSessionSystem::removeEntity(std::uint32_t networkId, const ecs::EntityRef &entity)
{
    const bool killed = _killed.erase(networkId) > 0;
    if (!_ecs.isAlive(entity)) {
        return;
    }
    const game::EntityKind kind = _ecs.getComponent<Replicated>(entity.id).kind;
    if (killed) {
        showDamage(entity.id, _ecs.getComponent<Replicated>(entity.id).health);
    }
    if (killed && _impactLooks.hasLook(kind)) {
        const ecs::Position position = _ecs.getComponent<ecs::Position>(entity.id);
        const ecs::entity_t impact = _impactLooks.createImpact(_ecs, *_scene, kind, position);
        _destructionSounds.addSound(_ecs, impact, kind, position.x);
    }
    _ecs.destroyEntity(entity.id);
}

void GameSessionSystem::moveEntity(ecs::entity_t entity, const game::EntityState &state)
{
    _ecs.getComponent<ecs::Position>(entity) = {state.x, state.y};
    Replicated &replicated = _ecs.getComponent<Replicated>(entity);
    const std::uint8_t health = std::exchange(replicated.health, state.health);
    replicated.maxHealth = std::max(replicated.maxHealth, state.health);
    if (state.health < health) {
        showDamage(entity, static_cast<std::uint8_t>(health - state.health));
    }
    if (state.health != health) {
        showHealth(entity, state, state.health < health);
    }
    if (_ecs.hasComponent<ShipAttitude>(entity)) {
        _shipLooks.moveModel(_ecs, entity, state, _clock);
    } else if (_ecs.hasComponent<BossModel>(entity)) {
        _bossLooks.moveModel(_ecs, entity, state);
    } else if (_ecs.hasComponent<engine::Model>(entity)) {
        _shotLooks.placeModel(_ecs, entity, state);
    }
}

void GameSessionSystem::showDamage(ecs::entity_t entity, std::uint8_t damage)
{
    const game::EntityKind kind = _ecs.getComponent<Replicated>(entity).kind;
    if (damage > 0 && _damageLooks.hasLook(kind)) {
        _damageLooks.createNumber(_ecs, *_scene, kind, _ecs.getComponent<ecs::Position>(entity), damage);
    }
}

void GameSessionSystem::showHealth(ecs::entity_t entity, const game::EntityState &state, bool hit)
{
    if (!_healthBarLooks.hasLook(state.kind) || isLocalShip(state)) {
        return;
    }
    std::optional<ecs::entity_t> bar = _ecs.getComponent<Replicated>(entity).healthBar;
    if (!bar && !hit) {
        return;
    }
    if (!bar) {
        bar = _healthBarLooks.createBar(_ecs, *_scene, state.kind, entity);
        _ecs.getComponent<Replicated>(entity).healthBar = bar;
    }
    HealthBar &health = _ecs.getComponent<HealthBar>(*bar);
    const std::uint8_t maxHealth = _ecs.getComponent<Replicated>(entity).maxHealth;
    health.health = maxHealth > 0 ? static_cast<float>(state.health) / static_cast<float>(maxHealth) : 0.0f;
    if (hit) {
        health.time = 0.0f;
    }
}

bool GameSessionSystem::isLocalShip(const game::EntityState &state) const
{
    return state.kind == game::EntityKind::PlayerShip && _session && state.owner == _session->localSlot();
}

void GameSessionSystem::placeShipModels(float elapsed)
{
    for (const auto &[networkId, entity] : _entities) {
        if (!_ecs.isAlive(entity)) {
            continue;
        }
        if (_ecs.hasComponent<ShipAttitude>(entity.id)) {
            ShipLooks::placeModel(_ecs, entity.id, elapsed);
        } else if (_ecs.hasComponent<BossModel>(entity.id)) {
            BossLooks::placeModel(_ecs, entity.id);
        }
    }
}

} // namespace rtype::client
