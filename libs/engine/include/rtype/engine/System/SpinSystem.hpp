#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"

namespace rtype::engine
{

/// Turns the models that have a Spin, by their speed times the elapsed time. The angle stays between 0 and
/// 360 degrees, so that it keeps its precision however long the game runs.
class SpinSystem : public ecs::System
{
    ecs::ComponentCache<Spin> &_spins;
    ecs::ComponentCache<Model> &_models;

public:
    explicit SpinSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
