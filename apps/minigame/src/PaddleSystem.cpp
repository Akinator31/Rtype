#include "PaddleSystem.hpp"

namespace orbit
{

using rtype::engine::Control;

PaddleSystem::PaddleSystem(rtype::ecs::ComponentCaches &caches, const rtype::engine::Scenes &scenes,
                           const rtype::engine::Controls &controls)
    : _scenes(scenes), _controls(controls), _paddles(use<Paddle>(caches)),
      _velocities(use<rtype::physics::Velocity>(caches))
{
}

void PaddleSystem::update(float /*elapsed*/)
{
    const float direction = _controls.getStrength(Control::Right) - _controls.getStrength(Control::Left);
    for (rtype::ecs::entity_t paddle = 0; paddle < _paddles.size(); ++paddle) {
        if (_paddles.has(paddle) && _velocities.has(paddle)) {
            _velocities.get(paddle).x = _scenes.receivesInput(paddle) ? direction * PaddleSpeed : 0.0f;
        }
    }
}

} // namespace orbit
