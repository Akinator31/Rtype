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
/// it sees the whole game (computeGameView()); while the shogun dashes at the formation
/// (game::RescueStage::Dash) it closes up on it, follows it a little late, leaning into its dashes, and
/// shakes at each ship its blade goes through.
struct RescueCamera {
    /// The part of the game the camera sees at rest.
    render::Rectangle area;
    /// How close up it is, from 0 (at rest) to 1.
    float closeUp = 0.0f;
    /// The point of the game it frames when close up: it follows the shogun.
    render::Vector2 focus;
    /// How much it shakes, from 0 to 1: a strike adds to it (shakeCamera()), then it calms down.
    float shake = 0.0f;
    /// Seconds it has been moved: the time of its shake.
    float time = 0.0f;
};

/// Shakes a camera as the blade of the shogun going through a ship does.
void shakeCamera(RescueCamera &camera);

/// Moves the cameras of the entities with a RescueCamera and an engine::Camera each frame, from the stage of
/// the rescue and the shogun drawn by the GameSessionSystem: after it, which moves the shogun.
class RescueCameraSystem : public ecs::System
{
    const GameSessionSystem &_sessions;
    ecs::ComponentCache<RescueCamera> &_rescueCameras;
    ecs::ComponentCache<engine::Camera> &_cameras;
    ecs::ComponentCache<Replicated> &_replicated;
    ecs::ComponentCache<ecs::Position> &_positions;

    /// Where the shogun is drawn, if it is.
    [[nodiscard]] std::optional<ecs::Position> findBoss() const;

public:
    RescueCameraSystem(ecs::ComponentCaches &caches, const GameSessionSystem &sessions);

    void update(float elapsed) override;
};

} // namespace rtype::client
