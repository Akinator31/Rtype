#include "Game/GameSessionSystem.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Control/Control.hpp"
#include "rtype/game/PlayerInput.hpp"
#include "rtype/game/Rules.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
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

/// How a kind of entity is drawn. The snapshots do not carry the sizes of the shots and of the Bydos: those
/// of the configuration of the repository (config/ships/, config/projectiles/), which a ShipLook fits its
/// model to.
struct KindLook {
    game::EntityKind kind;
    float width;
    float height;
    /// Drawn in the colour of its player rather than in `color`.
    bool playerColored;
    render::Color color;
    int order;
};

const std::array<KindLook, 5> KindLooks{{
    {game::EntityKind::PlayerShip, DefaultRules.shipHalfWidth * 2.0f, DefaultRules.shipHalfHeight * 2.0f,
     true, {}, ShipOrder},
    {game::EntityKind::PlayerShot, 24.0f, 8.0f, true, {}, ShotOrder},
    {game::EntityKind::PlayerChargedShot, 48.0f, 16.0f, true, {}, ShotOrder},
    {game::EntityKind::BydoFlyer, 100.0f, 38.0f, false, {171, 71, 188, 255}, ShipOrder},
    {game::EntityKind::BydoShot, 16.0f, 16.0f, false, {255, 112, 67, 255}, ShotOrder},
}};

/// The other kinds come with the gameplay: a grey box until they have a look.
const KindLook UnknownLook{game::EntityKind::BydoBoss, 48.0f, 48.0f, false, render::colors::Gray, ShipOrder};

/// The ships are seen from the side, and a little from above. The camera of the game looks with y pointing
/// down: a half turn around the x axis puts the top of a model (+y) up on the screen, and its side towards
/// the camera; this many more degrees show its top.
constexpr float HalfTurn = 180.0f;
constexpr float ShipTilt = 20.0f;
/// The length of the ship models, around which they are tilted and roll.
constexpr render::Vector3 LengthAxis{1.0f, 0.0f, 0.0f};

/// How far in front of the plane of the game the camera is: further than the depth of any model.
constexpr float GameViewDistance = 1000.0f;

/// A ship at full vertical speed rolls by this many degrees around its length: its belly towards the camera
/// while it climbs, further, its top while it dives.
constexpr float MaxClimbRoll = 40.0f;
constexpr float MaxDiveRoll = 25.0f;
/// How fast a ship reaches its roll: this fraction of the gap per second, about.
constexpr float RollResponse = 10.0f;

constexpr float RadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

/// The roll a ship turns towards: its vertical speed, as a fraction of the speed of the ships of the players.
float computeTargetRoll(const game::EntityState &state)
{
    const float climb = std::clamp(-state.velocityY / DefaultRules.shipSpeed, -1.0f, 1.0f);
    return -climb * (climb > 0.0f ? MaxClimbRoll : MaxDiveRoll);
}

/// Seconds for the flame of a ship to grow or shrink when it starts or stops flying forward.
constexpr float FlameFade = 0.25f;

/// Whether a ship flies forward: to the right for the players, to the left for the Bydos. Only then has it
/// flames, animated; otherwise it has none, even while it climbs or dives.
bool isFlyingForward(const ShipLook &ship, const game::EntityState &state)
{
    return ship.facesLeft ? state.velocityX < 0.0f : state.velocityX > 0.0f;
}

/// The model of a ship in the colour of its player, lightened so that its details still show.
render::Color tintOf(const EntityLook &look)
{
    const auto lighten = [](std::uint8_t channel) {
        return static_cast<std::uint8_t>((channel + 255) / 2);
    };
    return {lighten(look.box.color.r), lighten(look.box.color.g), lighten(look.box.color.b), 255};
}

} // namespace

render::Camera computeGameView(render::Rectangle visibleArea)
{
    const float centreX = visibleArea.x + (visibleArea.width / 2.0f);
    const float centreY = visibleArea.y + (visibleArea.height / 2.0f);
    return {{centreX, centreY, -GameViewDistance},
            {centreX, centreY, 0.0f},
            {0.0f, -1.0f, 0.0f},
            visibleArea.height,
            render::Projection::Orthographic};
}

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
    const auto found = std::ranges::find(KindLooks, entity.kind, &KindLook::kind);
    const KindLook &look = found != KindLooks.end() ? *found : UnknownLook;
    const render::Color color = look.playerColored ? playerColor(entity.owner) : look.color;
    return {{look.width, look.height, color}, look.order};
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
    use<engine::Model>(caches);
    use<engine::ModelAnimation>(caches);
    use<ShipAttitude>(caches);
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
    _clock = 0.0f;
}

void GameSessionSystem::setShipLook(game::EntityKind kind, const ShipLook &look)
{
    _shipLooks.insert_or_assign(kind, look);
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
    placeModels(elapsed);
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
        const ecs::entity_t entity = found->second.id;
        _ecs.getComponent<ecs::Position>(entity) = {state.x, state.y};
        if (_ecs.hasComponent<engine::Model>(entity)) {
            moveModel(entity, state);
        }
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

void GameSessionSystem::createEntity(const game::EntityState &state)
{
    const EntityLook look = lookOf(state);
    const ecs::entity_t entity = _ecs.createEntity();
    _ecs.addComponent<engine::SceneMember>(entity, *_scene);
    _ecs.addComponent<Replicated>(entity, state.id);
    _ecs.addComponent<ecs::Position>(entity, state.x, state.y);
    _ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, look.order);
    const auto ship = _shipLooks.find(state.kind);
    if (ship != _shipLooks.end()) {
        addModel(entity, state, look, ship->second);
    } else {
        _ecs.addComponent<engine::Box>(entity, look.box);
    }
    _entities.insert_or_assign(state.id, _ecs.ref(entity));
}

void GameSessionSystem::addModel(ecs::entity_t entity, const game::EntityState &state, const EntityLook &look,
                                 const ShipLook &ship)
{
    const bool isPlayerShip = state.kind == game::EntityKind::PlayerShip;
    const render::Rectangle &hull = ship.hull;
    _ecs.addComponent<engine::Model>(entity, ship.model.model, render::Vector3{}, LengthAxis,
                                     HalfTurn + ShipTilt, look.box.width / hull.width,
                                     isPlayerShip ? tintOf(look) : render::colors::White,
                                     ship.facesLeft ? HalfTurn : 0.0f);
    _ecs.addComponent<engine::ModelAnimation>(
        entity, isFlyingForward(ship, state) ? ship.model.animations.move : ship.model.animations.idle);
    const render::Vector2 hullCentre{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)};
    _ecs.addComponent<ShipAttitude>(entity, hullCentre);
    moveModel(entity, state);
    placeModel(entity, 0.0f);
}

void GameSessionSystem::moveModel(ecs::entity_t entity, const game::EntityState &state)
{
    _ecs.getComponent<ShipAttitude>(entity).targetRoll = computeTargetRoll(state);
    engine::ModelAnimation &animation = _ecs.getComponent<engine::ModelAnimation>(entity);
    const ShipLook &ship = _shipLooks.at(state.kind);
    const bool forward = isFlyingForward(ship, state);
    _ecs.getComponent<engine::Model>(entity).hiddenMeshes = forward ? std::vector<int>{} : ship.flameMeshes;
    const int next = forward ? ship.model.animations.move : ship.model.animations.idle;
    if (animation.animation != next) {
        engine::switchModelAnimation(animation, next, FlameFade);
    } else if (animation.time < animation.fadeDuration) {
        return;
    } else if (forward) {
        // In step with the game: the ships of a model that fly alike share their pose, posed once a frame
        animation = {next, _clock};
    } else {
        // Without flames, the ship holds still where the fade left it: not posed again
        animation = {next, animation.time, next, 0.0f, 0.0f, 0.0f};
    }
}

void GameSessionSystem::placeModel(ecs::entity_t entity, float elapsed)
{
    ShipAttitude &attitude = _ecs.getComponent<ShipAttitude>(entity);
    attitude.roll += (attitude.targetRoll - attitude.roll) * (1.0f - std::exp(-RollResponse * elapsed));

    // Around the axis of the tilt: the roll adds to it
    engine::Model &model = _ecs.getComponent<engine::Model>(entity);
    model.rotationAngle = HalfTurn + ShipTilt + attitude.roll;
    // The centre of the hull on the entity: it rolls around it
    const float roll = attitude.roll * RadiansPerDegree;
    const ecs::Position &position = _ecs.getComponent<ecs::Position>(entity);
    model.position = {position.x - (attitude.hullCentre.x * model.scale),
                      position.y - (attitude.hullCentre.y * model.scale * std::cos(roll)), 0.0f};
}

void GameSessionSystem::placeModels(float elapsed)
{
    for (const auto &[networkId, entity] : _entities) {
        if (_ecs.isAlive(entity) && _ecs.hasComponent<ShipAttitude>(entity.id)) {
            placeModel(entity.id, elapsed);
        }
    }
}

} // namespace rtype::client
