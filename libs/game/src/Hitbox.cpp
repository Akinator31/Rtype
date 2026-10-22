#include "rtype/game/Hitbox.hpp"

#include "rtype/physics/Geometry/Intersections.hpp"

#include <variant>

namespace rtype::game
{

namespace
{

using Shape = decltype(physics::Collider::shape);

/// The shape of a collider, moved to the position of its entity.
Shape placeShape(const ecs::Position &position, const physics::Collider &collider)
{
    Shape shape = collider.shape;
    std::visit(
        [&](auto &placed) {
            placed.center.x += position.x + collider.offset.x;
            placed.center.y += position.y + collider.offset.y;
        },
        shape);
    return shape;
}

} // namespace

physics::Vec3 halfSizeOf(const physics::Collider &collider)
{
    if (const auto *box = std::get_if<physics::Aabb>(&collider.shape)) {
        return box->halfSize;
    }
    const float radius = std::get<physics::Sphere>(collider.shape).radius;
    return {radius, radius, radius};
}

bool overlaps(const ecs::Position &first, const physics::Collider &firstCollider, const ecs::Position &second,
              const physics::Collider &secondCollider)
{
    return std::visit([](const auto &a, const auto &b) { return physics::intersects(a, b); },
                      placeShape(first, firstCollider), placeShape(second, secondCollider));
}

} // namespace rtype::game
