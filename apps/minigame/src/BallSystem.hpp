#pragma once

#include "Bounce.hpp"
#include "Breaker.hpp"
#include "Looks.hpp"
#include "rtype/audio/AudioPlayer.hpp"
#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Control/Controls.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/physics/Component/Motion.hpp"
#include "rtype/physics/System/CollisionSystem.hpp"

namespace orbit
{

/// The sounds of the ball.
struct BallSounds {
    rtype::audio::SoundId bounce;
    rtype::audio::SoundId brick;
    rtype::audio::SoundId lost;
};

/// Runs the ball, after the physics: it rides on the paddle until Fire launches it, bounces on what the
/// CollisionSystem reports (walls, bricks, and the paddle, which aims it), and is lost under the paddle,
/// which costs a life. A brick hit pops, scores more for each brick hit since the paddle (the combo), and
/// bursts once broken; the ball leaves a trail of light.
class BallSystem : public rtype::ecs::System
{
    rtype::ecs::Ecs &_ecs;
    const rtype::engine::Scenes &_scenes;
    const rtype::engine::Controls &_controls;
    const rtype::physics::CollisionSystem &_collisions;
    Match &_match;
    rtype::audio::AudioPlayer &_audio;
    BallSounds _sounds;
    const Looks &_looks;
    rtype::ecs::ComponentCache<Ball> &_balls;
    rtype::ecs::ComponentCache<Paddle> &_paddles;
    rtype::ecs::ComponentCache<Brick> &_bricks;
    rtype::ecs::ComponentCache<rtype::ecs::Position> &_positions;
    rtype::ecs::ComponentCache<rtype::physics::Velocity> &_velocities;
    rtype::ecs::ComponentCache<rtype::engine::Box> &_boxes;
    rtype::ecs::ComponentCache<rtype::engine::SceneMember> &_members;

    [[nodiscard]] const rtype::ecs::Position *findPaddle() const;
    void bounce(const rtype::physics::Collision &collision);
    void hitPaddle(rtype::ecs::entity_t ball, rtype::ecs::entity_t paddle);
    void hitObstacle(rtype::ecs::entity_t ball, rtype::ecs::entity_t obstacle, Vector normal, float depth);
    void hitBrick(rtype::ecs::entity_t ball, rtype::ecs::entity_t brick);
    void speedUpBall(rtype::ecs::entity_t ball);
    void rideOnPaddle(rtype::ecs::entity_t ball, Ball &state);
    void loseBall(rtype::ecs::entity_t ball, Ball &state);

public:
    /// @param ecs, scenes, controls, collisions, match, audio, looks They must outlive the system.
    BallSystem(rtype::ecs::ComponentCaches &caches, rtype::ecs::Ecs &ecs, const rtype::engine::Scenes &scenes,
               const rtype::engine::Controls &controls, const rtype::physics::CollisionSystem &collisions,
               Match &match, rtype::audio::AudioPlayer &audio, BallSounds sounds, const Looks &looks);

    void update(float elapsed) override;
};

} // namespace orbit
