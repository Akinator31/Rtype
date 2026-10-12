#include "rtype/physics/System/PhysicsStep.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rtype::physics
{

PhysicsStep::PhysicsStep(ecs::ComponentCaches &caches, float step, std::size_t maxSteps)
    : _caches(caches), _step(step), _maxSteps(maxSteps)
{
    if (!std::isfinite(step) || step <= 0.0f || maxSteps == 0) {
        throw std::invalid_argument("PhysicsStep requires a positive finite step and maxSteps");
    }
}

void PhysicsStep::update(float elapsed)
{
    if (!std::isfinite(elapsed) || elapsed < 0.0f) {
        throw std::invalid_argument("PhysicsStep requires a nonnegative finite elapsed time");
    }

    const double step = static_cast<double>(_step);
    const double tolerance = step * 0.00001;
    _accumulator += static_cast<double>(elapsed);

    std::size_t steps = 0;
    while (_accumulator + tolerance >= step && steps < _maxSteps) {
        for (auto &system : _systems) {
            if (system->enabled) {
                system->update(_step);
            }
        }
        _accumulator = std::max(0.0, _accumulator - step);
        ++steps;
    }

    if (steps == _maxSteps && _accumulator >= step) {
        _accumulator = std::fmod(_accumulator, step);
    }
}

float PhysicsStep::step() const
{
    return _step;
}

std::size_t PhysicsStep::maxSteps() const
{
    return _maxSteps;
}

float PhysicsStep::alpha() const
{
    return static_cast<float>(_accumulator / static_cast<double>(_step));
}

} // namespace rtype::physics
