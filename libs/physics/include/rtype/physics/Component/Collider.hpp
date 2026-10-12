#pragma once

#include "rtype/physics/Geometry/Shapes.hpp"

#include <variant>

namespace rtype::physics
{

struct Collider {
    std::variant<Aabb, Sphere> shape;
    Vec3 offset;
    bool trigger = false;
};

}
