#include "Game/GameSessionSystem.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Control/Control.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"

#include <array>
#include <unordered_set>
#include <utility>

namespace rtype::client
{

namespace
{

/// The sizes of the rules, so that what is drawn is what collides.
const game::Rules DefaultRules{};

/// Drawn above the background, the shots under the ships.
constexpr int ShotOrder = 10;
constexpr int ShipOrder = 20;

} // namespace

render::Color playerColor(std::uint8_t slot)
{
    static constexpr std::array<render::Color, 4> Colors = {{
        {79, 195, 247, 255},  // blue
        {239, 83, 80, 255},   // red
        {102, 187, 106, 255}, // green
        {255, 202, 40, 255},  // yellow
    }};
    return slot < Colors.size() ? Colors[slot] : render::colors::White;
}

EntityLook lookOf(const game::EntityState &entity)
{
    switch (entity.kind) {
    case game::EntityKind::PlayerShip:
        return {{DefaultRules.shipHalfWidth * 2.0f, DefaultRules.shipHalfHeight * 2.0f,
                 playerColor(entity.owner)},
                ShipOrder};
    case game::EntityKind::PlayerShot:
    case game::EntityKind::PlayerChargedShot:
        return {{DefaultRules.shotHalfWidth * 2.0f, DefaultRules.shotHalfHeight * 2.0f,
                 playerColor(entity.owner)},
                ShotOrder};
    default:
        // The Bydos and their shots come with the gameplay: a box until they have a look
        return {{48.0f, 48.0f, render::colors::Gray}, ShipOrder};
    }
}

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
    use<engine::Drawable>(caches);
    use<engine::SceneMember>(caches);
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
    _players.clear();
    _tick = 0;
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
    for (const game::Snapshot &snapshot : _session->takeSnapshots()) {
        apply(snapshot);
    }
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
        if (found != _entities.end() && _ecs.isAlive(found->second)) {
            _ecs.getComponent<ecs::Position>(found->second.id) = {state.x, state.y};
            continue;
        }
        const EntityLook look = lookOf(state);
        const ecs::entity_t entity = _ecs.createEntity();
        _ecs.addComponent<engine::SceneMember>(entity, *_scene);
        _ecs.addComponent<Replicated>(entity, state.id);
        _ecs.addComponent<ecs::Position>(entity, state.x, state.y);
        _ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, look.order);
        _ecs.addComponent<engine::Box>(entity, look.box);
        _entities.insert_or_assign(state.id, _ecs.ref(entity));
    }

    // Each snapshot holds the whole world: what is not in it any more is gone
    for (auto entry = _entities.begin(); entry != _entities.end();) {
        if (present.contains(entry->first)) {
            ++entry;
            continue;
        }
        _ecs.destroyEntity(entry->second.id);
        entry = _entities.erase(entry);
    }
}

} // namespace rtype::client
