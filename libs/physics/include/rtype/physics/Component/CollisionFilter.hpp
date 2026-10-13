#pragma once

#include <cstdint>

namespace rtype::physics
{

struct CollisionLayer {
    std::uint32_t bits = ~std::uint32_t{0};
};

struct CollisionMask {
    std::uint32_t bits = ~std::uint32_t{0};
};

struct CollisionFilter {
    const CollisionLayer *layer = nullptr;
    const CollisionMask *mask = nullptr;
};

[[nodiscard]] bool canCollide(CollisionFilter a, CollisionFilter b) noexcept;

} // namespace rtype::physics
