#include "rtype/physics/Component/CollisionFilter.hpp"

namespace rtype::physics
{

bool canCollide(CollisionFilter a, CollisionFilter b) noexcept
{
    const std::uint32_t aLayer = a.layer ? a.layer->bits : ~std::uint32_t{0};
    const std::uint32_t aMask = a.mask ? a.mask->bits : ~std::uint32_t{0};
    const std::uint32_t bLayer = b.layer ? b.layer->bits : ~std::uint32_t{0};
    const std::uint32_t bMask = b.mask ? b.mask->bits : ~std::uint32_t{0};
    return (aMask & bLayer) != 0 && (bMask & aLayer) != 0;
}

}
