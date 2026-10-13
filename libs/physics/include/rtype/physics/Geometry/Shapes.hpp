#pragma once

namespace rtype::physics
{

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Aabb {
    Vec3 center;
    Vec3 halfSize;
};

struct Sphere {
    Vec3 center;
    float radius = 0.0f;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

} // namespace rtype::physics
