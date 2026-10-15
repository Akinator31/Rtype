#include "rtype/physics/Geometry/Intersections.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rtype::physics
{

namespace
{

Vec3 subtract(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 scale(Vec3 value, float factor)
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

float dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 halfSize(const Aabb &box)
{
    return {std::max(0.0f, box.halfSize.x), std::max(0.0f, box.halfSize.y),
            std::max(0.0f, box.halfSize.z)};
}

float radius(const Sphere &sphere)
{
    return std::max(0.0f, sphere.radius);
}

Vec3 closestPoint(const Aabb &box, Vec3 point)
{
    const Vec3 half = halfSize(box);
    return {std::clamp(point.x, box.center.x - half.x, box.center.x + half.x),
            std::clamp(point.y, box.center.y - half.y, box.center.y + half.y),
            std::clamp(point.z, box.center.z - half.z, box.center.z + half.z)};
}

bool planar(const Aabb &box, const Aabb &other)
{
    return box.halfSize.z == 0.0f && other.halfSize.z == 0.0f &&
           box.center.z == other.center.z;
}

bool planar(const Aabb &box, const Sphere &sphere)
{
    return box.halfSize.z == 0.0f && box.center.z == sphere.center.z;
}

}

bool contains(const Aabb &box, Vec3 point)
{
    const Vec3 half = halfSize(box);
    const Vec3 delta = subtract(point, box.center);
    return std::abs(delta.x) <= half.x && std::abs(delta.y) <= half.y &&
           std::abs(delta.z) <= half.z;
}

bool contains(const Sphere &sphere, Vec3 point)
{
    const Vec3 delta = subtract(point, sphere.center);
    const float r = radius(sphere);
    return dot(delta, delta) <= r * r;
}

bool intersects(const Aabb &a, const Aabb &b)
{
    const Vec3 ah = halfSize(a);
    const Vec3 bh = halfSize(b);
    const Vec3 delta = subtract(b.center, a.center);
    return std::abs(delta.x) <= ah.x + bh.x && std::abs(delta.y) <= ah.y + bh.y &&
           std::abs(delta.z) <= ah.z + bh.z;
}

bool intersects(const Sphere &a, const Sphere &b)
{
    const Vec3 delta = subtract(b.center, a.center);
    const float r = radius(a) + radius(b);
    return dot(delta, delta) <= r * r;
}

bool intersects(const Aabb &box, const Sphere &sphere)
{
    const Vec3 delta = subtract(sphere.center, closestPoint(box, sphere.center));
    const float r = radius(sphere);
    return dot(delta, delta) <= r * r;
}

bool intersects(const Sphere &sphere, const Aabb &box)
{
    return intersects(box, sphere);
}

std::optional<RayHit> raycast(const Ray &ray, const Aabb &box)
{
    const Vec3 half = halfSize(box);
    const float origin[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};
    const float minimum[3] = {box.center.x - half.x, box.center.y - half.y,
                              box.center.z - half.z};
    const float maximum[3] = {box.center.x + half.x, box.center.y + half.y,
                              box.center.z + half.z};

    float entry = 0.0f;
    float exit = std::numeric_limits<float>::infinity();
    Vec3 normal;
    for (int axis = 0; axis < 3; ++axis) {
        if (direction[axis] == 0.0f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) {
                return std::nullopt;
            }
            continue;
        }

        const float near = ((direction[axis] > 0.0f ? minimum[axis] : maximum[axis]) -
                            origin[axis]) / direction[axis];
        const float far = ((direction[axis] > 0.0f ? maximum[axis] : minimum[axis]) -
                           origin[axis]) / direction[axis];
        if (near >= entry) {
            entry = near;
            normal = {};
            const float sign = direction[axis] > 0.0f ? -1.0f : 1.0f;
            if (axis == 0) {
                normal.x = sign;
            } else if (axis == 1) {
                normal.y = sign;
            } else {
                normal.z = sign;
            }
        }
        exit = std::min(exit, far);
        if (entry > exit) {
            return std::nullopt;
        }
    }

    if (exit < 0.0f) {
        return std::nullopt;
    }
    return RayHit{entry, add(ray.origin, scale(ray.direction, entry)), normal};
}

std::optional<RayHit> raycast(const Ray &ray, const Sphere &sphere)
{
    if (contains(sphere, ray.origin)) {
        return RayHit{0.0f, ray.origin, {}};
    }

    const Vec3 offset = subtract(ray.origin, sphere.center);
    const float a = dot(ray.direction, ray.direction);
    if (a == 0.0f) {
        return std::nullopt;
    }
    const float b = dot(offset, ray.direction);
    const float r = radius(sphere);
    const float discriminant = b * b - a * (dot(offset, offset) - r * r);
    if (discriminant < 0.0f) {
        return std::nullopt;
    }
    const float t = (-b - std::sqrt(discriminant)) / a;
    if (t < 0.0f) {
        return std::nullopt;
    }
    const Vec3 point = add(ray.origin, scale(ray.direction, t));
    const Vec3 normal = r > 0.0f ? scale(subtract(point, sphere.center), 1.0f / r) : Vec3{};
    return RayHit{t, point, normal};
}

std::optional<Penetration> penetration(const Aabb &a, const Aabb &b)
{
    if (!intersects(a, b)) {
        return std::nullopt;
    }
    const Vec3 ah = halfSize(a);
    const Vec3 bh = halfSize(b);
    const Vec3 delta = subtract(b.center, a.center);
    const float overlap[3] = {ah.x + bh.x - std::abs(delta.x),
                              ah.y + bh.y - std::abs(delta.y),
                              ah.z + bh.z - std::abs(delta.z)};
    const float difference[3] = {delta.x, delta.y, delta.z};
    const int dimensions = planar(a, b) ? 2 : 3;
    int axis = 0;
    for (int i = 1; i < dimensions; ++i) {
        if (overlap[i] < overlap[axis]) {
            axis = i;
        }
    }
    const float sign = difference[axis] < 0.0f ? -1.0f : 1.0f;
    Vec3 normal;
    if (axis == 0) {
        normal.x = sign;
    } else if (axis == 1) {
        normal.y = sign;
    } else {
        normal.z = sign;
    }
    return Penetration{normal, overlap[axis]};
}

std::optional<Penetration> penetration(const Sphere &a, const Sphere &b)
{
    const Vec3 delta = subtract(b.center, a.center);
    const float distanceSquared = dot(delta, delta);
    const float combinedRadius = radius(a) + radius(b);
    if (distanceSquared > combinedRadius * combinedRadius) {
        return std::nullopt;
    }
    const float distance = std::sqrt(distanceSquared);
    const Vec3 normal = distance > 0.0f ? scale(delta, 1.0f / distance) : Vec3{1.0f, 0.0f, 0.0f};
    return Penetration{normal, combinedRadius - distance};
}

std::optional<Penetration> penetration(const Aabb &box, const Sphere &sphere)
{
    const Vec3 closest = closestPoint(box, sphere.center);
    const Vec3 delta = subtract(sphere.center, closest);
    const float distanceSquared = dot(delta, delta);
    const float r = radius(sphere);
    if (distanceSquared > r * r) {
        return std::nullopt;
    }
    if (distanceSquared > 0.0f) {
        const float distance = std::sqrt(distanceSquared);
        return Penetration{scale(delta, 1.0f / distance), r - distance};
    }

    const Vec3 half = halfSize(box);
    const Vec3 relative = subtract(sphere.center, box.center);
    const float face[3] = {half.x - std::abs(relative.x), half.y - std::abs(relative.y),
                           half.z - std::abs(relative.z)};
    const float difference[3] = {relative.x, relative.y, relative.z};
    const int dimensions = planar(box, sphere) ? 2 : 3;
    int axis = 0;
    for (int i = 1; i < dimensions; ++i) {
        if (face[i] < face[axis]) {
            axis = i;
        }
    }
    const float sign = difference[axis] < 0.0f ? -1.0f : 1.0f;
    Vec3 normal;
    if (axis == 0) {
        normal.x = sign;
    } else if (axis == 1) {
        normal.y = sign;
    } else {
        normal.z = sign;
    }
    return Penetration{normal, r + face[axis]};
}

std::optional<Penetration> penetration(const Sphere &sphere, const Aabb &box)
{
    const auto result = penetration(box, sphere);
    if (!result) {
        return std::nullopt;
    }
    return Penetration{scale(result->normal, -1.0f), result->depth};
}

}
