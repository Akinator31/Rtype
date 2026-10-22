#include "Game/GameSessionSystem.hpp"

#include "Game/EntityLook.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Control/Control.hpp"
#include "rtype/game/PlayerInput.hpp"

#include <unordered_set>
#include <utility>

namespace rtype::client
{

game::PlayerInput inputFrom(const engine::Controls &controls)
{
    game::PlayerInput input;
    input.set(game::PlayerInput::Up, controls.isDown(engine::Control::Up));
    input.set(game::PlayerInput::Down, controls.isDown(engine::Control::Down));
    input.set(game::PlayerInput::Left, controls.isDown(engine::Control::Left));
    input.set(game::PlayerInput::Right, controls.isDown(engine::Control::Right));
    input.set(game::PlayerInput::Fire, controls.isDown(engine::Control::FaceDown));
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
    use<engine::Drawable>(caches);
    use<engine::SceneMember>(caches);
    use<Impact>(caches);
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
    _clock = 0.0f;
}

void GameSessionSystem::setShipLook(game::EntityKind kind, const ShipLook &look)
{
    _shipLooks.setLook(kind, look);
}

void GameSessionSystem::setShotLook(game::EntityKind kind, const ShotLook &look)
{
    _shotLooks.setLook(kind, look);
}

void GameSessionSystem::setImpactLook(game::EntityKind kind, const ImpactLook &look)
{
    _impactLooks.setLook(kind, look);
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
    rememberKilled(snapshot.destroyed);

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

void GameSessionSystem::rememberKilled(const std::vector<game::DestroyedEntity> &destroyed)
{
    for (const game::DestroyedEntity &entity : destroyed) {
        if (entity.cause == game::DestroyCause::Killed && _entities.contains(entity.id)) {
            _killed.insert(entity.id);
        }
    }
}

void GameSessionSystem::createEntity(const game::EntityState &state)
{
    const EntityLook look = lookOf(state);
    const ecs::entity_t entity = _ecs.createEntity();
    _ecs.addComponent<engine::SceneMember>(entity, *_scene);
    _ecs.addComponent<Replicated>(entity, state.id, state.kind);
    _ecs.addComponent<ecs::Position>(entity, state.x, state.y);
    _ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, look.order);
    if (_shipLooks.hasLook(state.kind)) {
        _shipLooks.addModel(_ecs, entity, state, look, _clock);
    } else if (_shotLooks.hasLook(state.kind)) {
        _shotLooks.addModel(_ecs, entity, state, look);
    } else {
        _ecs.addComponent<engine::Box>(entity, look.box);
    }
    _entities.insert_or_assign(state.id, _ecs.ref(entity));
}

void GameSessionSystem::removeEntity(std::uint32_t networkId, const ecs::EntityRef &entity)
{
    const bool killed = _killed.erase(networkId) > 0;
    if (!_ecs.isAlive(entity)) {
        return;
    }
    const game::EntityKind kind = _ecs.getComponent<Replicated>(entity.id).kind;
    if (killed && _impactLooks.hasLook(kind)) {
        _impactLooks.createImpact(_ecs, *_scene, kind, _ecs.getComponent<ecs::Position>(entity.id));
    }
    _ecs.destroyEntity(entity.id);
}

void GameSessionSystem::moveEntity(ecs::entity_t entity, const game::EntityState &state)
{
    _ecs.getComponent<ecs::Position>(entity) = {state.x, state.y};
    if (_ecs.hasComponent<ShipAttitude>(entity)) {
        _shipLooks.moveModel(_ecs, entity, state, _clock);
    } else if (_ecs.hasComponent<engine::Model>(entity)) {
        _shotLooks.placeModel(_ecs, entity, state);
    }
}

void GameSessionSystem::placeShipModels(float elapsed)
{
    for (const auto &[networkId, entity] : _entities) {
        if (_ecs.isAlive(entity) && _ecs.hasComponent<ShipAttitude>(entity.id)) {
            ShipLooks::placeModel(_ecs, entity.id, elapsed);
        }
    }
}

} // namespace rtype::client
