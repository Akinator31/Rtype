#include "Game/RescueCameraSystem.hpp"

#include "Game/GameView.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rtype::client
{

namespace
{

/// How much of the game the camera sees when close up, as a share of what it sees at rest.
constexpr float CloseUpZoom = 0.62f;
/// How fast the camera closes up and goes back, and how fast it follows the shogun: the share of the way left
/// it goes in a second is 1 - e^-rate.
constexpr float CloseUpRate = 2.5f;
constexpr float FollowRate = 5.0f;
/// How much it leans into a dash, in degrees, once the shogun is LeanDistance game units ahead of its focus.
constexpr float MaxLean = 4.0f;
constexpr float LeanDistance = 400.0f;
/// What a strike adds to the shake, and how fast the shake calms down, per second.
constexpr float StrikeShake = 0.75f;
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

float smooth(float amount)
{
    return amount * amount * (3.0f - (2.0f * amount));
}

/// A shake from -1 to 1 at a time: two waves of unrelated frequencies, so that it does not look regular.
float wobble(float time, float first, float second)
{
    return (0.6f * std::sin(time * first)) + (0.4f * std::sin((time * second) + 1.3f));
}

/// The part of the game a camera sees when this close up, around its focus, kept inside what it sees at rest.
render::Rectangle frame(const RescueCamera &camera, float closeUp, float punch)
{
    const render::Rectangle &area = camera.area;
    const float zoom = (1.0f + ((CloseUpZoom - 1.0f) * closeUp)) * (1.0f - punch);
    const float width = area.width * zoom;
    const float height = area.height * zoom;
    const float centreX = area.x + (area.width / 2.0f);
    const float centreY = area.y + (area.height / 2.0f);
    const float x = std::clamp(centreX + ((camera.focus.x - centreX) * closeUp), area.x + (width / 2.0f),
                               area.x + area.width - (width / 2.0f));
    const float y = std::clamp(centreY + ((camera.focus.y - centreY) * closeUp), area.y + (height / 2.0f),
                               area.y + area.height - (height / 2.0f));
    return {x - (width / 2.0f), y - (height / 2.0f), width, height};
}

/// The view of a camera: its frame, shaken, and turned around its axis by its lean into the dash of the
/// shogun and its shake.
render::Camera computeView(const RescueCamera &camera, const std::optional<ecs::Position> &boss)
{
    const float closeUp = smooth(camera.closeUp);
    const float strength = camera.shake * camera.shake;
    render::Camera view = computeGameView(frame(camera, closeUp, strength * ShakePunch));
    const float shakeX = strength * ShakeDistance * wobble(camera.time, 37.0f, 59.0f);
    const float shakeY = strength * ShakeDistance * wobble(camera.time + 3.0f, 43.0f, 67.0f);
    view.position.x += shakeX;
    view.position.y += shakeY;
    view.target.x += shakeX;
    view.target.y += shakeY;
    const float ahead = boss ? (boss->x - camera.focus.x) / LeanDistance : 0.0f;
    const float lean = std::clamp(ahead, -1.0f, 1.0f) * MaxLean * closeUp;
    const float roll = (lean + (strength * ShakeRoll * wobble(camera.time + 7.0f, 31.0f, 53.0f))) *
                       std::numbers::pi_v<float> / 180.0f;
    // At rest, up is -y: the camera of the game looks with y pointing down
    view.up = {std::sin(roll), -std::cos(roll), 0.0f};
    return view;
}

} // namespace

void shakeCamera(RescueCamera &camera)
{
    camera.shake = std::min(camera.shake + StrikeShake, 1.0f);
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

void RescueCameraSystem::update(float elapsed)
{
    const std::optional<ecs::Position> boss = findBoss();
    const bool closeUp = boss && _sessions.rescue() == game::RescueStage::Dash;
    for (ecs::entity_t entity = 0; entity < _rescueCameras.size(); ++entity) {
        if (!_rescueCameras.has(entity) || !_cameras.has(entity)) {
            continue;
        }
        RescueCamera &camera = _rescueCameras.get(entity);
        camera.time += elapsed;
        camera.closeUp = approach(camera.closeUp, closeUp ? 1.0f : 0.0f, CloseUpRate, elapsed);
        if (boss) {
            camera.focus = {approach(camera.focus.x, boss->x, FollowRate, elapsed),
                            approach(camera.focus.y, boss->y, FollowRate, elapsed)};
        }
        camera.shake = std::max(camera.shake - (ShakeDecay * elapsed), 0.0f);
        _cameras.get(entity).view = computeView(camera, boss);
    }
}

} // namespace rtype::client
