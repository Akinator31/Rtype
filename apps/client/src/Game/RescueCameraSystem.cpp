#include "Game/RescueCameraSystem.hpp"

#include "Game/GameView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace rtype::client
{

namespace
{

/// What a shot frames.
enum class Subject {
    /// Nothing: the camera stays on its focus.
    None,
    /// The shogun: the whole game instead while it is not drawn.
    Boss,
    /// The ship of the local player, or where it was last drawn.
    Ship,
};

/// What the camera shows during a stage of the rescue.
struct Shot {
    /// How much of the game it sees, as a share of what it sees at rest.
    float zoom = 1.0f;
    /// How much it is turned around its axis, in degrees.
    float tilt = 0.0f;
    /// How fast it gets there: the share of the way left it goes in a second is 1 - e^-rate.
    float rate = 2.5f;
    /// The shake it keeps at the least, from 0 to 1.
    float rumble = 0.0f;
    /// What it frames.
    Subject subject = Subject::None;
};

/// At rest, the whole game; back there when the allies arrive, and after the dashes, before the prompt.
constexpr Shot Rest{};
/// The voice of the rescue: a slow push in on the ship of the player, just lost.
constexpr Shot VoiceShot{.zoom = 0.5f, .rate = 1.2f, .subject = Subject::Ship};
/// The dashes: close up on the shogun.
constexpr Shot DashShot{.zoom = 0.62f, .subject = Subject::Boss};
/// The combined attack: a slow push in, closer, tilted, rumbling under the fire.
constexpr Shot CombinedShot{
    .zoom = 0.48f, .tilt = -3.0f, .rate = 0.9f, .rumble = 0.45f, .subject = Subject::Boss};
/// The fall of the shogun: a slow pull back from its explosion to the whole game.
constexpr Shot FallShot{.rate = 0.6f};

/// The shot of each stage of the rescue, in the order of game::RescueStage: None, Voice, Arrival, Assault,
/// Dash, Prompt, Combined, Finished.
constexpr std::array<Shot, static_cast<std::size_t>(game::RescueStage::Finished) + 1> Shots{
    {Rest, VoiceShot, Rest, Rest, DashShot, Rest, CombinedShot, FallShot}};

/// How fast the camera follows its subject, as Shot::rate.
constexpr float FollowRate = 5.0f;
/// How much it leans into a dash, in degrees, once its subject is LeanDistance game units ahead of its focus.
constexpr float MaxLean = 4.0f;
constexpr float LeanDistance = 400.0f;
/// How fast the shake calms down, per second.
constexpr float ShakeDecay = 1.6f;
/// At full shake: how far the camera goes, in game units, how much it turns, in degrees, and how much closer
/// it gets, as a share of what it sees.
constexpr float ShakeDistance = 24.0f;
constexpr float ShakeRoll = 1.5f;
constexpr float ShakePunch = 0.06f;

float approach(float value, float target, float rate, float elapsed)
{
    return value + ((target - value) * (1.0f - std::exp(-rate * elapsed)));
}

/// The shot of a stage of the rescue: close ups on the shogun only while it is drawn.
Shot shotOf(game::RescueStage stage, bool boss)
{
    const Shot &shot = Shots.at(static_cast<std::size_t>(stage));
    return shot.subject == Subject::Boss && !boss ? Rest : shot;
}

/// Where the subject of a shot is, if it has one.
std::optional<render::Vector2> findSubject(Subject subject, const RescueCamera &camera,
                                           const std::optional<ecs::Position> &boss)
{
    if (subject == Subject::Boss && boss) {
        return render::Vector2{boss->x, boss->y};
    }
    if (subject == Subject::Ship) {
        return camera.ship;
    }
    return std::nullopt;
}

/// A shake from -1 to 1 at a time: two waves of unrelated frequencies, so that it does not look regular.
float wobble(float time, float first, float second)
{
    return (0.6f * std::sin(time * first)) + (0.4f * std::sin((time * second) + 1.3f));
}

/// The part of the game a camera sees, around its focus, kept inside what it sees at rest: at rest, the whole
/// game whatever its focus.
render::Rectangle frame(const RescueCamera &camera, float punch)
{
    const render::Rectangle &area = camera.area;
    const float zoom = camera.zoom * (1.0f - punch);
    const float width = area.width * zoom;
    const float height = area.height * zoom;
    const float x = std::clamp(camera.focus.x, area.x + (width / 2.0f), area.x + area.width - (width / 2.0f));
    const float y =
        std::clamp(camera.focus.y, area.y + (height / 2.0f), area.y + area.height - (height / 2.0f));
    return {x - (width / 2.0f), y - (height / 2.0f), width, height};
}

/// The view of a camera: its frame, shaken, and turned around its axis by its tilt, its lean towards its
/// subject running ahead of it (the shogun dashing), as much as it is close up, and its shake.
render::Camera computeView(const RescueCamera &camera, const std::optional<render::Vector2> &subject)
{
    const float closeUp = std::clamp((1.0f - camera.zoom) / (1.0f - DashShot.zoom), 0.0f, 1.0f);
    const float strength = camera.shake * camera.shake;
    render::Camera view = computeGameView(frame(camera, strength * ShakePunch));
    const float shakeX = strength * ShakeDistance * wobble(camera.time, 37.0f, 59.0f);
    const float shakeY = strength * ShakeDistance * wobble(camera.time + 3.0f, 43.0f, 67.0f);
    view.position.x += shakeX;
    view.position.y += shakeY;
    view.target.x += shakeX;
    view.target.y += shakeY;
    const float ahead = subject ? (subject->x - camera.focus.x) / LeanDistance : 0.0f;
    const float lean = std::clamp(ahead, -1.0f, 1.0f) * MaxLean * closeUp;
    const float roll =
        (camera.tilt + lean + (strength * ShakeRoll * wobble(camera.time + 7.0f, 31.0f, 53.0f))) *
        std::numbers::pi_v<float> / 180.0f;
    // At rest, up is -y: the camera of the game looks with y pointing down
    view.up = {std::sin(roll), -std::cos(roll), 0.0f};
    return view;
}

} // namespace

void shakeCamera(RescueCamera &camera, float strength)
{
    camera.shake = std::min(camera.shake + strength, 1.0f);
}

RescueCameraSystem::RescueCameraSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions)
    : _sessions(sessions), _rescueCameras(use<RescueCamera>(caches)), _cameras(use<engine::Camera>(caches)),
      _replicated(use<Replicated>(caches)), _positions(use<ecs::Position>(caches)),
      _models(use<engine::Model>(caches))
{
}

void RescueCameraSystem::arrive(RescueCamera &camera, engine::Camera &view, std::optional<ecs::entity_t> ship,
                                float elapsed)
{
    *camera.arrival += elapsed;
    const float progress = std::clamp(*camera.arrival / ArrivalDuration, 0.0f, 1.0f);
    const float amount = progress * progress * (3.0f - (2.0f * progress));
    const render::Rectangle from = computeArrivalFrame(camera.area, camera.ship);
    const render::Rectangle &to = camera.area;
    const auto mix = [amount](float first, float second) { return first + ((second - first) * amount); };
    view.view = computeGameView(
        {mix(from.x, to.x), mix(from.y, to.y), mix(from.width, to.width), mix(from.height, to.height)});

    // The ship was white in the hangar: it takes the colour of its player
    if (ship && _models.has(*ship)) {
        engine::Model &model = _models.get(*ship);
        if (!camera.shipTint) {
            camera.shipTint = model.tint;
        }
        const render::Color tint = *camera.shipTint;
        const auto channel = [&mix](std::uint8_t colour) {
            return static_cast<std::uint8_t>(std::lround(mix(255.0f, static_cast<float>(colour))));
        };
        model.tint = {channel(tint.r), channel(tint.g), channel(tint.b), tint.a};
    }
    if (progress >= 1.0f) {
        camera.arrival.reset();
        camera.focus = camera.ship;
    }
}

std::optional<ecs::Position> RescueCameraSystem::findBoss() const
{
    for (ecs::entity_t entity = 0; entity < _replicated.size(); ++entity) {
        if (_replicated.has(entity) && _positions.has(entity) &&
            _replicated.get(entity).kind == game::EntityKind::BydoBoss) {
            return _positions.get(entity);
        }
    }
    return std::nullopt;
}

std::optional<ecs::entity_t> RescueCameraSystem::findLocalShip() const
{
    const GameSession *session = _sessions.session();
    if (session == nullptr) {
        return std::nullopt;
    }
    for (ecs::entity_t entity = 0; entity < _replicated.size(); ++entity) {
        if (_replicated.has(entity) && _positions.has(entity) &&
            _replicated.get(entity).kind == game::EntityKind::PlayerShip &&
            _replicated.get(entity).owner == session->localSlot()) {
            return entity;
        }
    }
    return std::nullopt;
}

void RescueCameraSystem::update(float elapsed)
{
    const std::optional<ecs::Position> boss = findBoss();
    const std::optional<ecs::entity_t> ship = findLocalShip();
    const Shot shot = shotOf(_sessions.rescue(), boss.has_value());
    for (ecs::entity_t entity = 0; entity < _rescueCameras.size(); ++entity) {
        if (!_rescueCameras.has(entity) || !_cameras.has(entity)) {
            continue;
        }
        RescueCamera &camera = _rescueCameras.get(entity);
        camera.time += elapsed;
        camera.zoom = approach(camera.zoom, shot.zoom, shot.rate, elapsed);
        camera.tilt = approach(camera.tilt, shot.tilt, shot.rate, elapsed);
        if (ship) {
            camera.ship = {_positions.get(*ship).x, _positions.get(*ship).y};
        }
        if (camera.arrival) {
            arrive(camera, _cameras.get(entity), ship, elapsed);
            continue;
        }
        const std::optional<render::Vector2> subject = findSubject(shot.subject, camera, boss);
        if (subject) {
            camera.focus = {approach(camera.focus.x, subject->x, FollowRate, elapsed),
                            approach(camera.focus.y, subject->y, FollowRate, elapsed)};
        }
        camera.shake = std::max(camera.shake - (ShakeDecay * elapsed), shot.rumble);
        _cameras.get(entity).view = computeView(camera, subject);
    }
}

} // namespace rtype::client
