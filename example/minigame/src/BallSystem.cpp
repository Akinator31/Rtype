#include "BallSystem.hpp"

#include "Effects.hpp"
#include "Levels.hpp"
#include "Visuals.hpp"

#include <cmath>

namespace orbit
{

namespace ecs = rtype::ecs;

namespace
{

/// Where the ball rides on the paddle, and where it is put back on it after a bounce: just above it.
constexpr float rideHeight = PaddleY - ((PaddleHeight + BallSize) / 2.0f);
/// The ball leaves the paddle slightly to the right when launched, never straight up.
constexpr float launchOffset = 0.15f;
constexpr rtype::audio::PlaySettings bounceVolume{.volume = 0.35f};

} // namespace

BallSystem::BallSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const rtype::engine::Scenes &scenes,
                       const rtype::engine::Controls &controls,
                       const rtype::physics::CollisionSystem &collisions, Match &match,
                       rtype::audio::AudioPlayer &audio, BallSounds sounds, const Looks &looks)
    : _ecs(ecs), _scenes(scenes), _controls(controls), _collisions(collisions), _match(match), _audio(audio),
      _sounds(sounds), _looks(looks), _balls(use<Ball>(caches)), _paddles(use<Paddle>(caches)),
      _bricks(use<Brick>(caches)), _positions(use<ecs::Position>(caches)),
      _velocities(use<rtype::physics::Velocity>(caches)), _boxes(use<rtype::engine::Box>(caches)),
      _members(use<rtype::engine::SceneMember>(caches))
{
}

const ecs::Position *BallSystem::findPaddle() const
{
    for (ecs::entity_t paddle = 0; paddle < _paddles.size(); ++paddle) {
        if (_paddles.has(paddle) && _positions.has(paddle)) {
            return &_positions.get(paddle);
        }
    }
    return nullptr;
}

void BallSystem::bounce(const rtype::physics::Collision &collision)
{
    const bool ballFirst = _balls.has(collision.a.id);
    const ecs::EntityRef ball = ballFirst ? collision.a : collision.b;
    const ecs::EntityRef other = ballFirst ? collision.b : collision.a;
    if (!_balls.has(ball.id) || !_balls.get(ball.id).launched || !_ecs.isAlive(other)) {
        return;
    }
    if (_paddles.has(other.id)) {
        hitPaddle(ball.id, other.id);
        return;
    }
    // The normal goes from the first entity to the second: turned to go from the ball to what it hits
    const float sign = ballFirst ? 1.0f : -1.0f;
    hitObstacle(ball.id, other.id, {collision.normal.x * sign, collision.normal.y * sign}, collision.depth);
}

void BallSystem::hitPaddle(ecs::entity_t ball, ecs::entity_t paddle)
{
    rtype::physics::Velocity &velocity = _velocities.get(ball);
    if (velocity.y <= 0.0f) {
        return;
    }
    ecs::Position &position = _positions.get(ball);
    const float offset = (position.x - _positions.get(paddle).x) / (PaddleWidth / 2.0f);
    const Vector leaving = leavePaddle(offset, _balls.get(ball).speed);
    velocity = {leaving.x, leaving.y};
    position.y = rideHeight;
    _match.combo = 0;
    popIn(_ecs, paddle);
    _audio.play(_sounds.bounce, bounceVolume);
}

void BallSystem::hitObstacle(ecs::entity_t ball, ecs::entity_t obstacle, Vector normal, float depth)
{
    rtype::physics::Velocity &velocity = _velocities.get(ball);
    const Vector bounced = reflect({velocity.x, velocity.y}, normal);
    // Already going away: a second brick of the same contact
    if (bounced == Vector{velocity.x, velocity.y}) {
        return;
    }
    velocity = {bounced.x, bounced.y};
    ecs::Position &position = _positions.get(ball);
    position.x -= normal.x * depth;
    position.y -= normal.y * depth;
    if (_bricks.has(obstacle)) {
        hitBrick(ball, obstacle);
    } else {
        _audio.play(_sounds.bounce, bounceVolume);
    }
}

void BallSystem::speedUpBall(ecs::entity_t ball)
{
    Ball &state = _balls.get(ball);
    rtype::physics::Velocity &velocity = _velocities.get(ball);
    const float speed = speedUp(state.speed);
    velocity = {velocity.x * speed / state.speed, velocity.y * speed / state.speed};
    state.speed = speed;
}

void BallSystem::hitBrick(ecs::entity_t ball, ecs::entity_t brick)
{
    const int points = hitPoints(++_match.combo);
    _match.score += points;
    speedUpBall(ball);
    _audio.play(_sounds.brick);
    const ecs::Position where = _positions.get(brick);
    const rtype::engine::SceneId scene = _members.get(brick).scene;
    spawnPoints(_ecs, scene, _looks, where.x, where.y, points);
    Brick &hit = _bricks.get(brick);
    if (--hit.strength <= 0) {
        spawnBurst(_ecs, scene, _looks, where.x, where.y, _boxes.get(brick).color);
        _ecs.destroyEntity(brick);
        return;
    }
    rtype::engine::Box &look = _boxes.get(brick);
    look.color = brickColor(hit.strength);
    look.bottom = brickBottom(hit.strength);
    popIn(_ecs, brick);
}

void BallSystem::rideOnPaddle(ecs::entity_t ball, Ball &state)
{
    const ecs::Position *paddle = findPaddle();
    if (paddle == nullptr) {
        return;
    }
    _positions.get(ball) = {paddle->x, rideHeight};
    rtype::physics::Velocity &velocity = _velocities.get(ball);
    velocity = {};
    if (_scenes.receivesInput(ball) && _controls.wasPressed(rtype::engine::Control::FaceDown)) {
        state.launched = true;
        _match.ballWaiting = false;
        const Vector leaving = leavePaddle(launchOffset, state.speed);
        velocity = {leaving.x, leaving.y};
    }
}

void BallSystem::loseBall(ecs::entity_t ball, Ball &state)
{
    --_match.lives;
    _match.combo = 0;
    _match.ballWaiting = true;
    state = Ball{};
    _velocities.get(ball) = {};
    _audio.play(_sounds.lost);
}

void BallSystem::update(float /*elapsed*/)
{
    for (const rtype::physics::Collision &collision : _collisions.collisions()) {
        bounce(collision);
    }
    for (ecs::entity_t ball = 0; ball < _balls.size(); ++ball) {
        if (!_balls.has(ball) || !_scenes.updates(ball)) {
            continue;
        }
        Ball &state = _balls.get(ball);
        if (!state.launched) {
            rideOnPaddle(ball, state);
        } else if (_positions.get(ball).y > ArenaHeight + BallSize) {
            loseBall(ball, state);
        } else {
            spawnTrail(_ecs, _members.get(ball).scene, _positions.get(ball).x, _positions.get(ball).y);
        }
    }
}

} // namespace orbit
