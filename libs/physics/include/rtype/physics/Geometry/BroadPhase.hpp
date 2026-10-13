#pragma once

#include "rtype/physics/Geometry/Shapes.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace rtype::physics
{

struct BroadPhasePair {
    std::size_t first = 0;
    std::size_t second = 0;

    friend bool operator==(const BroadPhasePair &, const BroadPhasePair &) = default;
};

struct BroadPhaseResult {
    std::vector<BroadPhasePair> pairs;
    std::size_t testedPairs = 0;
    std::size_t occupiedCells = 0;
};

[[nodiscard]] BroadPhaseResult broadPhaseNaive(std::span<const Aabb> bounds);
[[nodiscard]] BroadPhaseResult broadPhaseGrid(std::span<const Aabb> bounds, float cellSize);

}
