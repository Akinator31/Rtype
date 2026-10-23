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

/// The flames of a ship, whatever it does: a little flame, halfway between the Idle pose of the rig (0),
/// whose flames hide in the thrusters, and the Move one (1).
constexpr float Flame = 0.5f;

/// The roll a ship turns towards: its vertical speed, as a fraction of the speed of the ships of the players.
float computeTargetRoll(const game::EntityState &state)
{
    const float climb = std::clamp(-state.velocityY / DefaultRules.shipSpeed, -1.0f, 1.0f);
    return -climb * (climb > 0.0f ? MaxClimbRoll : MaxDiveRoll);
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
                                    ship.facesLeft ? HalfTurn : 0.0f)
        .hiddenMeshes = ship.trailMeshes;
    engine::ModelAnimation &animation =
        ecs.addComponent<engine::ModelAnimation>(entity, ship.model.animations.idle);
    animation.blendedAnimation = ship.model.animations.move;
    animation.blendAmount = Flame;
    const render::Vector2 hullCentre{hull.x + (hull.width / 2.0f), hull.y + (hull.height / 2.0f)};
    ecs.addComponent<ShipAttitude>(entity, hullCentre);
    moveModel(ecs, entity, state, clock);
    placeModel(ecs, entity, 0.0f);
}

void ShipLooks::moveModel(ecs::Ecs &ecs, ecs::entity_t entity, const game::EntityState &state,
                          float clock) const
{
    ecs.getComponent<ShipAttitude>(entity).targetRoll = computeTargetRoll(state);
    // In step with the game: the ships of a model share their pose, posed once a frame
    ecs.getComponent<engine::ModelAnimation>(entity).time = clock;
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
