#include "Game/ShipLooks.hpp"

#include "Game/GameView.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/game/Rules.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace rtype::client
{

namespace
{

/// The speed of the ships of the players, to which their vertical speed is compared.
const game::Rules DefaultRules{};

/// The ships are seen from the side, and a little from above: this many degrees more than the half turn
/// around their length show their top.
constexpr float ShipTilt = 20.0f;

/// A ship at full vertical speed rolls by this many degrees around its length: its belly towards the camera
/// while it climbs, further, its top while it dives.
constexpr float MaxClimbRoll = 40.0f;
constexpr float MaxDiveRoll = 25.0f;
/// How fast a ship reaches its roll: this fraction of the gap per second, about.
constexpr float RollResponse = 10.0f;

constexpr float RadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

/// Seconds for the flame of a ship to grow or shrink when it starts or stops flying forward.
constexpr float FlameFade = 0.25f;

/// The roll a ship turns towards: its vertical speed, as a fraction of the speed of the ships of the players.
float computeTargetRoll(const game::EntityState &state)
{
    const float climb = std::clamp(-state.velocityY / DefaultRules.shipSpeed, -1.0f, 1.0f);
    return -climb * (climb > 0.0f ? MaxClimbRoll : MaxDiveRoll);
}

/// Whether a ship flies forward: to the right for the players, to the left for the Bydos. Only then has it
/// flames, animated; otherwise it has none, even while it climbs or dives.
bool isFlyingForward(const ShipLook &ship, const game::EntityState &state)
{
    return ship.facesLeft ? state.velocityX < 0.0f : state.velocityX > 0.0f;
}

/// The model of a ship in the colour of its player, lightened so that its details still show.
render::Color tintOf(const EntityLook &look)
{
    const auto lighten = [](std::uint8_t channel) { return static_cast<std::uint8_t>((channel + 255) / 2); };
    return {lighten(look.box.color.r), lighten(look.box.color.g), lighten(look.box.color.b), 255};
}

} // namespace

void ShipLooks::setLook(game::EntityKind kind, const ShipLook &look)
{
    _looks.insert_or_assign(kind, look);
}

bool ShipLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

void ShipLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                         const EntityLook &look, float clock) const
{
    const ShipLook &ship = _looks.at(state.kind);
    const bool isPlayerShip = state.kind == game::EntityKind::PlayerShip;
    const render::Rectangle &hull = ship.hull;
    ecs.addComponent<engine::Model>(entity, ship.model.model, render::Vector3{}, LengthAxis,
                                    HalfTurn + ShipTilt, look.box.width / hull.width,
                                    isPlayerShip ? tintOf(look) : render::colors::White,
                                    ship.facesLeft ? HalfTurn : 0.0f);
    ecs.addComponent<engine::ModelAnimation>(
        entity, isFlyingForward(ship, state) ? ship.model.animations.move : ship.model.animations.idle);
    const render::Vector2 hullCentre{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)};
    ecs.addComponent<ShipAttitude>(entity, hullCentre);
    moveModel(ecs, entity, state, clock);
    placeModel(ecs, entity, 0.0f);
}

void ShipLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                          float clock) const
{
    ecs.getComponent<ShipAttitude>(entity).targetRoll = computeTargetRoll(state);
    engine::ModelAnimation &animation = ecs.getComponent<engine::ModelAnimation>(entity);
    const ShipLook &ship = _looks.at(state.kind);
    const bool forward = isFlyingForward(ship, state);
    ecs.getComponent<engine::Model>(entity).hiddenMeshes = forward ? std::vector<int>{} : ship.flameMeshes;
    const int next = forward ? ship.model.animations.move : ship.model.animations.idle;
    if (animation.animation != next) {
        engine::switchModelAnimation(animation, next, FlameFade);
    } else if (animation.time < animation.fadeDuration) {
        return;
    } else if (forward) {
        // In step with the game: the ships of a model that fly alike share their pose, posed once a frame
        animation = {next, clock};
    } else {
        // Without flames, the ship holds still where the fade left it: not posed again
        animation = {next, animation.time, next, 0.0f, 0.0f, 0.0f};
    }
}

void ShipLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity, float elapsed)
{
    ShipAttitude &attitude = ecs.getComponent<ShipAttitude>(entity);
    attitude.roll += (attitude.targetRoll - attitude.roll) * (1.0f - std::exp(-RollResponse * elapsed));

    // Around the axis of the tilt: the roll adds to it
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    model.rotationAngle = HalfTurn + ShipTilt + attitude.roll;
    // The centre of the hull on the entity: it rolls around it
    const float roll = attitude.roll * RadiansPerDegree;
    const ecs::Position &position = ecs.getComponent<ecs::Position>(entity);
    model.position = {position.x - (attitude.hullCentre.x * model.scale),
                      position.y - (attitude.hullCentre.y * model.scale * std::cos(roll)), 0.0f};
}

} // namespace rtype::client
