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
#include <tuple>
#include <utility>

namespace rtype::client
{

namespace
{

/// The speed of the ships of the players, to which their vertical speed is compared.
const game::Rules DefaultRules{};

/// A ship at full vertical speed rolls by this many degrees around its length: its belly towards the camera
/// while it climbs, further, its top while it dives.
constexpr float MaxClimbRoll = 40.0f;
constexpr float MaxDiveRoll = 25.0f;
/// How fast a ship reaches its roll: this fraction of the gap per second, about.
constexpr float RollResponse = 10.0f;
/// How fast a ship reaches its aim, the same way: quicker, the shots already fly that way.
constexpr float AimResponse = 20.0f;

constexpr float RadiansPerDegree = std::numbers::pi_v<float> / 180.0f;

/// The roll a ship turns towards: its vertical speed, as a fraction of the speed of the ships of the players.
float computeTargetRoll(const game::EntityState &state)
{
    const float climb = std::clamp(-state.velocityY / DefaultRules.shipSpeed, -1.0f, 1.0f);
    return -climb * (climb > 0.0f ? MaxClimbRoll : MaxDiveRoll);
}

/// The aim a ship turns towards: the angle of the rules, nose up or down as its flags say.
float computeTargetAim(const game::EntityState &state)
{
    if ((state.flags & game::EntityState::AimingUp) != 0) {
        return -DefaultRules.aimAngle;
    }
    if ((state.flags & game::EntityState::AimingDown) != 0) {
        return DefaultRules.aimAngle;
    }
    return 0.0f;
}

/// The rotation of the model as an axis and an angle in degrees: `angle` around the length (LengthAxis), then
/// `aim` in the plane of the game (around z, towards +y), composed as quaternions.
std::pair<render::Vector3, float> composeRotation(float angle, float aim)
{
    const float halfAngle = angle * RadiansPerDegree / 2.0f;
    const float halfAim = aim * RadiansPerDegree / 2.0f;
    const float w = std::cos(halfAim) * std::cos(halfAngle);
    const render::Vector3 vector{std::cos(halfAim) * std::sin(halfAngle),
                                 std::sin(halfAim) * std::sin(halfAngle),
                                 std::sin(halfAim) * std::cos(halfAngle)};
    const float length = std::hypot(vector.x, vector.y, vector.z);
    if (length == 0.0f) {
        return {LengthAxis, 0.0f};
    }
    return {{vector.x / length, vector.y / length, vector.z / length},
            2.0f * std::atan2(length, w) / RadiansPerDegree};
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
    _looks.insert_or_assign(kind, std::vector<ShipLook>{look});
}

void ShipLooks::setLooks(game::EntityKind kind, std::vector<ShipLook> looks)
{
    if (looks.empty()) {
        _looks.erase(kind);
        return;
    }
    _looks.insert_or_assign(kind, std::move(looks));
}

void ShipLooks::setLocalLook(const ShipLook &look)
{
    _localLook = look;
}

void ShipLooks::setShipLook(std::uint8_t ship, const ShipLook &look, render::Vector2 size)
{
    _shipLooks.insert_or_assign(ship, SizedLook{look, size});
}

bool ShipLooks::hasLook(game::EntityKind kind) const
{
    return _looks.contains(kind);
}

EntityLook ShipLooks::resizeToShip(const game::EntityState &state, EntityLook look) const
{
    const auto found = _shipLooks.find(state.ship);
    if (found != _shipLooks.end()) {
        look.box.width = found->second.size.x;
        look.box.height = found->second.size.y;
    }
    return look;
}

const ShipLook &ShipLooks::findLook(const game::EntityState &state, bool local) const
{
    if (local && _localLook) {
        return *_localLook;
    }
    const auto found = _shipLooks.find(state.ship);
    if (found != _shipLooks.end()) {
        return found->second.look;
    }
    const std::vector<ShipLook> &looks = _looks.at(state.kind);
    return looks[state.id % looks.size()];
}

void ShipLooks::addModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                         const EntityLook &look, float clock, bool local) const
{
    const ShipLook &ship = findLook(state, local);
    const bool isPlayerShip = state.kind == game::EntityKind::PlayerShip;
    const render::Rectangle &hull = ship.hull;
    ecs.addComponent<engine::Model>(entity, ship.model.model, render::Vector3{}, LengthAxis,
                                    HalfTurn + ShipTilt, look.box.width / hull.width,
                                    isPlayerShip ? tintOf(look) : render::colors::White,
                                    ship.facesLeft ? HalfTurn : 0.0f)
        .hiddenMeshes = ship.trailMeshes;
    engine::ModelAnimation &animation =
        ecs.addComponent<engine::ModelAnimation>(entity, ship.model.animations.idle);
    animation.blendedAnimation = ship.model.animations.move;
    animation.blendAmount = ShipFlame;
    const render::Vector2 hullCentre{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)};
    ShipAttitude &attitude = ecs.addComponent<ShipAttitude>(entity, hullCentre);
    attitude.shoot = ship.model.animations.shoot;
    attitude.shootDuration = ship.model.animations.shootDuration;
    moveModel(ecs, entity, state, clock);
    placeModel(ecs, entity, 0.0f);
}

void ShipLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                          float clock) const
{
    ShipAttitude &attitude = ecs.getComponent<ShipAttitude>(entity);
    attitude.targetRoll = computeTargetRoll(state);
    attitude.targetAim = computeTargetAim(state);
    // In step with the game: the ships of a model share their pose, posed once a frame
    ecs.getComponent<engine::ModelAnimation>(entity).time = clock;
}

void ShipLooks::placeModel(ecs::Ecs &ecs, ecs::entity_t entity, float elapsed)
{
    ShipAttitude &attitude = ecs.getComponent<ShipAttitude>(entity);
    attitude.roll += (attitude.targetRoll - attitude.roll) * (1.0f - std::exp(-RollResponse * elapsed));
    attitude.aim += (attitude.targetAim - attitude.aim) * (1.0f - std::exp(-AimResponse * elapsed));

    // Around the axis of the tilt: the roll adds to it; then the aim turns the whole ship in the plane
    engine::Model &model = ecs.getComponent<engine::Model>(entity);
    std::tie(model.rotationAxis, model.rotationAngle) =
        composeRotation(HalfTurn + ShipTilt + attitude.roll, attitude.aim);
    // The centre of the hull on the entity: it rolls and turns around it
    const float roll = attitude.roll * RadiansPerDegree;
    const float aim = attitude.aim * RadiansPerDegree;
    const render::Vector2 centre{attitude.hullCentre.x * model.scale,
                                 attitude.hullCentre.y * model.scale * std::cos(roll)};
    const ecs::Position &position = ecs.getComponent<ecs::Position>(entity);
    model.position = {position.x - ((centre.x * std::cos(aim)) - (centre.y * std::sin(aim))),
                      position.y - ((centre.x * std::sin(aim)) + (centre.y * std::cos(aim))), 0.0f};
}

void ShipLooks::fireModel(ecs::Ecs &ecs, ecs::entity_t entity)
{
    const ShipAttitude &attitude = ecs.getComponent<ShipAttitude>(entity);
    engine::addModelAnimation(ecs.getComponent<engine::ModelAnimation>(entity), attitude.shoot,
                              attitude.shootDuration);
}

} // namespace rtype::client
