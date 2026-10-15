#include "rtype/physics/System/MovementSystem.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rtype::physics
{

MovementSystem::MovementSystem(ecs::ComponentCaches &caches, std::function<bool(ecs::entity_t)> shouldUpdate)
    : _velocities(use<Velocity>(caches)), _accelerations(use<Acceleration>(caches)),
      _maxSpeeds(use<MaxSpeed>(caches)), _positions(use<ecs::Position>(caches)),
      _localPositions(use<ecs::LocalPosition>(caches)), _parents(use<ecs::Parent>(caches)),
      _statics(use<Static>(caches)),
      _shouldUpdate(std::move(shouldUpdate))
{
}

void MovementSystem::update(float elapsed)
{
    if (elapsed <= 0.0f) {
        return;
    }

    for (ecs::entity_t entity = 0; entity < _velocities.size(); ++entity) {
        if (!_velocities.has(entity) || _statics.has(entity) ||
            (_shouldUpdate && !_shouldUpdate(entity))) {
            continue;
        }

        const bool isChild = _parents.has(entity);
        if ((isChild && !_localPositions.has(entity)) || (!isChild && !_positions.has(entity))) {
            continue;
        }

        Velocity &velocity = _velocities.get(entity);
        if (_accelerations.has(entity)) {
            const Acceleration &acceleration = _accelerations.get(entity);
            velocity.x += acceleration.x * elapsed;
            velocity.y += acceleration.y * elapsed;
        }

        if (_maxSpeeds.has(entity)) {
            const float limit = std::max(0.0f, _maxSpeeds.get(entity).value);
            const float speed = std::hypot(velocity.x, velocity.y);
            if (speed > limit) {
                const float factor = limit / speed;
                velocity.x *= factor;
                velocity.y *= factor;
            }
        }

        if (isChild) {
            ecs::LocalPosition &position = _localPositions.get(entity);
            position.x += velocity.x * elapsed;
            position.y += velocity.y * elapsed;
        } else {
            ecs::Position &position = _positions.get(entity);
            position.x += velocity.x * elapsed;
            position.y += velocity.y * elapsed;
        }
    }
}

} // namespace rtype::physics
