#pragma once

#include "Game/GameSessionSystem.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/render/Types.hpp"

#include <optional>

namespace rtype::client
{

/// The camera of the game scene, with its engine::Camera, during the rescue against the final boss: at rest
/// it sees the whole game (computeGameView()).
///
/// - While the voice of the rescue plays (game::RescueStage::Voice), it pushes in slowly on the ship of the
///   local player, where it was just lost; it pulls back as the allies arrive.
/// - While the shogun dashes at the formation (game::RescueStage::Dash), it closes up on it, follows it a
///   little late, leaning into its dashes, and shakes at each ship its blade goes through.
/// - During the combined attack (Combined), it pushes in slowly on the shogun, closer than for the dashes,
///   tilted, rumbling under the fire; the fall of the shogun shakes it hard.
/// - Once the shogun has fallen (Finished), it pulls back slowly from its explosion to the whole game.
struct RescueCamera {
    /// The part of the game the camera sees at rest.
    render::Rectangle area;
    /// How much of it the camera sees, from 1 at rest down to its closest up.
    float zoom = 1.0f;
    /// How much it is turned around its axis, in degrees, its lean and its shake aside.
    float tilt = 0.0f;
    /// The point of the game it frames when close up: it follows the subject of its shot.
    render::Vector2 focus;
    /// Where the ship of the local player was last drawn: it is lost when the voice of the rescue plays.
    render::Vector2 ship;
    /// How much it shakes, from 0 to 1: a blow adds to it (shakeCamera()), then it calms down.
    float shake = 0.0f;
    /// Seconds it has been moved: the time of its shake.
    float time = 0.0f;
};

/// Shakes a camera by a blow of a strength from 0 to 1: the blade of the shogun through a ship, its fall.
void shakeCamera(RescueCamera &camera, float strength);

/// Moves the cameras of the entities with a RescueCamera and an engine::Camera each frame, from the stage of
/// the rescue, the shogun and the ship of the local player drawn by the GameSessionSystem: after it, which
/// moves them.
class RescueCameraSystem : public ecs::System
{
    const GameSessionSystem &_sessions;
    ecs::ComponentCache<RescueCamera> &_rescueCameras;
    ecs::ComponentCache<engine::Camera> &_cameras;
    ecs::ComponentCache<Replicated> &_replicated;
    ecs::ComponentCache<ecs::Position> &_positions;

    /// Where the shogun is drawn, if it is.
    [[nodiscard]] std::optional<ecs::Position> findBoss() const;
    /// Where the ship of the local player is drawn, if it is.
    [[nodiscard]] std::optional<ecs::Position> findLocalShip() const;

public:
    RescueCameraSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions);

    void update(float elapsed) override;
};

} // namespace rtype::client
