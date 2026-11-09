#include "RulesSystem.hpp"

#include "Arena.hpp"
#include "Effects.hpp"

#include <string>
#include <utility>

namespace orbit
{

namespace ecs = rtype::ecs;

RulesSystem::RulesSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, const rtype::engine::Scenes &scenes,
                         Match &match, const Looks &looks, std::function<void()> onGameOver)
    : _ecs(ecs), _scenes(scenes), _match(match), _looks(looks), _onGameOver(std::move(onGameOver)),
      _balls(use<Ball>(caches)), _bricks(use<Brick>(caches)),
      _members(use<rtype::engine::SceneMember>(caches))
{
}

bool RulesSystem::hasBricksLeft() const
{
    for (ecs::entity_t brick = 0; brick < _bricks.size(); ++brick) {
        if (_bricks.has(brick) && _scenes.updates(brick)) {
            return true;
        }
    }
    return false;
}

void RulesSystem::startNextLevel(ecs::entity_t ball)
{
    ++_match.level;
    _match.combo = 0;
    _match.ballWaiting = true;
    const rtype::engine::SceneId scene = _members.get(ball).scene;
    spawnBricks(_ecs, scene, _match.level);
    spawnBanner(_ecs, scene, _looks, "LEVEL " + std::to_string(_match.level));
    _balls.get(ball) = Ball{};
}

void RulesSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t ball = 0; ball < _balls.size(); ++ball) {
        if (!_balls.has(ball) || !_scenes.updates(ball)) {
            continue;
        }
        if (_match.lives > 0) {
            _over = false;
            if (!hasBricksLeft()) {
                startNextLevel(ball);
            }
        } else if (!_over) {
            _over = true;
            _onGameOver();
        }
    }
}

} // namespace orbit
