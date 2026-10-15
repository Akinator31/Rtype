#pragma once

#include "rtype/ecs/Hierarchy.hpp"

namespace rtype::physics
{

struct ConfinedToBounds {
    ecs::Position min;
    ecs::Position max;
};

struct DestroyOutOfBounds {
    ecs::Position min;
    ecs::Position max;
    float margin = 0.0f;
};

}
