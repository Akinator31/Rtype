#pragma once

#include "rtype/physics/Geometry/Shapes.hpp"

#include <optional>

namespace rtype::physics
{

struct RayHit {
    float t = 0.0f;
    Vec3 point;
    Vec3 normal;
};

struct Penetration {
    Vec3 normal;
    float depth = 0.0f;
};

[[nodiscard]] bool contains(const Aabb &box, Vec3 point);
[[nodiscard]] bool contains(const Sphere &sphere, Vec3 point);

[[nodiscard]] bool intersects(const Aabb &a, const Aabb &b);
[[nodiscard]] bool intersects(const Sphere &a, const Sphere &b);
[[nodiscard]] bool intersects(const Aabb &box, const Sphere &sphere);
[[nodiscard]] bool intersects(const Sphere &sphere, const Aabb &box);

[[nodiscard]] std::optional<RayHit> raycast(const Ray &ray, const Aabb &box);
[[nodiscard]] std::optional<RayHit> raycast(const Ray &ray, const Sphere &sphere);

[[nodiscard]] std::optional<Penetration> penetration(const Aabb &a, const Aabb &b);
[[nodiscard]] std::optional<Penetration> penetration(const Sphere &a, const Sphere &b);
[[nodiscard]] std::optional<Penetration> penetration(const Aabb &box, const Sphere &sphere);
[[nodiscard]] std::optional<Penetration> penetration(const Sphere &sphere, const Aabb &box);

} // namespace rtype::physics
