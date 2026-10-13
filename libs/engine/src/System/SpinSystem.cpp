#include "rtype/engine/System/SpinSystem.hpp"

#include <cmath>

namespace rtype::engine
{

namespace
{

constexpr float fullTurn = 360.0f;

} // namespace

SpinSystem::SpinSystem(ecs::ComponentCaches &caches)
    : _spins(use<Spin>(caches)), _models(use<Model>(caches))
{
}

void SpinSystem::update(float elapsed)
{
    for (ecs::entity_t entity = 0; entity < _spins.size(); ++entity) {
        if (!_spins.has(entity) || !_models.has(entity)) {
            continue;
        }
        float &angle = _models.get(entity).spinAngle;
        angle = std::fmod(angle + (_spins.get(entity).speed * elapsed), fullTurn);
        if (angle < 0.0f) {
            angle += fullTurn;
        }
    }
}

} // namespace rtype::engine
