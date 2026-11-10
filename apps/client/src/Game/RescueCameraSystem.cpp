#include "Game/RescueCameraSystem.hpp"

#include "Game/GameView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
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

/// The cutscene of the voice of the start of a game: the camera, in perspective, starts behind the ship and
/// above it, CutsceneStart, and turns slowly around it towards its side, CutsceneEnd, getting closer and
/// lower. It looks a little ahead of the ship, tilted.
struct CutsceneShot {
    /// How far it turns around the ship from its side, in degrees: towards its back if positive.
    float azimuth;
    /// How far from the ship, and how high above it, in game units.
    float distance;
    float height;
    /// How much it is turned around its axis, in degrees.
    float tilt;
};
constexpr CutsceneShot CutsceneStart{.azimuth = 60.0f, .distance = 430.0f, .height = 130.0f, .tilt = 4.0f};
constexpr CutsceneShot CutsceneEnd{.azimuth = 18.0f, .distance = 330.0f, .height = 55.0f, .tilt = -2.0f};
/// How far ahead of the ship the cutscene looks, in game units, and its vertical field of view, in degrees.
constexpr float CutsceneLookAhead = 70.0f;
constexpr float CutsceneFieldOfView = 40.0f;
/// The ticks at the end of the voice during which the camera flies back to the view of the game.
constexpr std::uint32_t CutsceneReturnTicks = 100;

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

float mix(float from, float to, float weight)
{
    return from + ((to - from) * weight);
}

render::Vector3 mix(render::Vector3 from, render::Vector3 to, float weight)
{
    return {mix(from.x, to.x, weight), mix(from.y, to.y, weight), mix(from.z, to.z, weight)};
}

float smoothstep(float value)
{
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - (2.0f * clamped));
}

float toRadians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0f;
}

/// The view of the cutscene around a point of the game, `progress` from 0 to 1 through it.
render::Camera computeCutsceneView(render::Vector2 focus, float progress)
{
    const float weight = smoothstep(progress);
    const float azimuth = toRadians(mix(CutsceneStart.azimuth, CutsceneEnd.azimuth, weight));
    const float distance = mix(CutsceneStart.distance, CutsceneEnd.distance, weight);
    const float height = mix(CutsceneStart.height, CutsceneEnd.height, weight);
    const float tilt = toRadians(mix(CutsceneStart.tilt, CutsceneEnd.tilt, weight));
    // The game is seen from -z, y pointing down: behind the ship is -x, above it -y
    render::Camera view;
    view.position = {focus.x - (distance * std::sin(azimuth)), focus.y - height,
                     -distance * std::cos(azimuth)};
    view.target = {focus.x + CutsceneLookAhead, focus.y, 0.0f};
    view.up = {std::sin(tilt), -std::cos(tilt), 0.0f};
    view.fieldOfView = CutsceneFieldOfView;
    view.projection = render::Projection::Perspective;
    return view;
}

/// A view of the game seen in perspective as it is seen through its orthographic camera, on the plane of the
/// game: to fly between the cutscene and the view of the game.
render::Camera toPerspective(render::Camera view)
{
    const float dx = view.target.x - view.position.x;
    const float dy = view.target.y - view.position.y;
    const float dz = view.target.z - view.position.z;
    const float distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
    view.fieldOfView =
        2.0f * std::atan(view.fieldOfView / (2.0f * distance)) * 180.0f / std::numbers::pi_v<float>;
    view.projection = render::Projection::Perspective;
    return view;
}

/// The view of a camera during the cutscene at a tick: the cutscene, then a flight from it to the view of
/// the game in its last ticks; the view of the game once it is over.
render::Camera blendCutscene(const RescueCamera &camera, const render::Camera &game, std::uint32_t tick)
{
    if (!camera.cutsceneFocus || tick >= camera.cutsceneEnd || tick < camera.cutsceneStart) {
        return game;
    }
    const std::uint32_t length = camera.cutsceneEnd - camera.cutsceneStart;
    const std::uint32_t returnStart =
        camera.cutsceneStart + (length > CutsceneReturnTicks ? length - CutsceneReturnTicks : 0);
    const float progress = std::min(static_cast<float>(tick - camera.cutsceneStart) /
                                        static_cast<float>(std::max(returnStart - camera.cutsceneStart, 1U)),
                                    1.0f);
    const render::Camera cutscene = computeCutsceneView(*camera.cutsceneFocus, progress);
    if (tick < returnStart) {
        return cutscene;
    }
    const float back = smoothstep(static_cast<float>(tick - returnStart) /
                                  static_cast<float>(camera.cutsceneEnd - returnStart));
    const render::Camera perspective = toPerspective(game);
    render::Camera view = cutscene;
    view.position = mix(cutscene.position, perspective.position, back);
    view.target = mix(cutscene.target, perspective.target, back);
    view.up = mix(cutscene.up, perspective.up, back);
    view.fieldOfView = mix(cutscene.fieldOfView, perspective.fieldOfView, back);
    return view;
}

} // namespace

void shakeCamera(RescueCamera &camera, float strength)
{
    camera.shake = std::min(camera.shake + strength, 1.0f);
}

RescueCameraSystem::RescueCameraSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions)
    : _sessions(sessions), _rescueCameras(use<RescueCamera>(caches)), _cameras(use<engine::Camera>(caches)),
      _replicated(use<Replicated>(caches)), _positions(use<ecs::Position>(caches))
{
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

std::optional<ecs::Position> RescueCameraSystem::findLocalShip() const
{
    const GameSession *session = _sessions.session();
    if (session == nullptr) {
        return std::nullopt;
    }
    for (ecs::entity_t entity = 0; entity < _replicated.size(); ++entity) {
        if (_replicated.has(entity) && _positions.has(entity) &&
            _replicated.get(entity).kind == game::EntityKind::PlayerShip &&
            _replicated.get(entity).owner == session->localSlot()) {
            return _positions.get(entity);
        }
    }
    return std::nullopt;
}

void RescueCameraSystem::update(float elapsed)
{
    const std::optional<ecs::Position> boss = findBoss();
    const std::optional<ecs::Position> ship = findLocalShip();
    const Shot shot = shotOf(_sessions.rescue(), boss.has_value());
    const std::uint32_t tick = _sessions.tick();
    for (ecs::entity_t entity = 0; entity < _rescueCameras.size(); ++entity) {
        if (!_rescueCameras.has(entity) || !_cameras.has(entity)) {
            continue;
        }
        RescueCamera &camera = _rescueCameras.get(entity);
        const bool cutscene = tick >= camera.cutsceneStart && tick < camera.cutsceneEnd;
        camera.time += elapsed;
        camera.zoom = approach(camera.zoom, shot.zoom, shot.rate, elapsed);
        camera.tilt = approach(camera.tilt, shot.tilt, shot.rate, elapsed);
        if (ship) {
            camera.ship = {ship->x, ship->y};
        }
        const std::optional<render::Vector2> subject = findSubject(shot.subject, camera, boss);
        if (subject) {
            camera.focus = {approach(camera.focus.x, subject->x, FollowRate, elapsed),
                            approach(camera.focus.y, subject->y, FollowRate, elapsed)};
        }
        camera.shake = std::max(camera.shake - (ShakeDecay * elapsed), shot.rumble);
        const render::Camera view = computeView(camera, subject);
        if (cutscene && ship) {
            const render::Vector2 seen{ship->x, ship->y};
            const render::Vector2 focus = camera.cutsceneFocus.value_or(seen);
            camera.cutsceneFocus = render::Vector2{approach(focus.x, seen.x, FollowRate, elapsed),
                                                   approach(focus.y, seen.y, FollowRate, elapsed)};
        }
        _cameras.get(entity).view = cutscene ? blendCutscene(camera, view, tick) : view;
    }
}

} // namespace rtype::client
