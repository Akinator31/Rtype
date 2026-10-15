#include "rtype/physics/Geometry/BroadPhase.hpp"

#include "rtype/physics/Geometry/Intersections.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace rtype::physics
{

namespace
{

struct Cell {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;

    friend bool operator==(const Cell &, const Cell &) = default;
};

struct CellHash {
    std::size_t operator()(Cell cell) const
    {
        std::size_t hash = std::hash<std::int64_t>{}(cell.x);
        hash ^= std::hash<std::int64_t>{}(cell.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<std::int64_t>{}(cell.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

struct PairHash {
    std::size_t operator()(BroadPhasePair pair) const
    {
        std::size_t hash = std::hash<std::size_t>{}(pair.first);
        hash ^= std::hash<std::size_t>{}(pair.second) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

std::optional<std::int64_t> cellIndex(double position, float cellSize)
{
    const double index = std::floor(position / static_cast<double>(cellSize));
    const double minimum = static_cast<double>(std::numeric_limits<std::int64_t>::min());
    const double maximum = static_cast<double>(std::numeric_limits<std::int64_t>::max());
    if (!std::isfinite(index) || index < minimum || index >= maximum) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(index);
}

struct CellRange {
    Cell minimum;
    Cell maximum;
};

std::optional<CellRange> cellRange(const Aabb &box, float cellSize)
{
    const double halfX = std::max(0.0, static_cast<double>(box.halfSize.x));
    const double halfY = std::max(0.0, static_cast<double>(box.halfSize.y));
    const double halfZ = std::max(0.0, static_cast<double>(box.halfSize.z));
    const auto minX = cellIndex(static_cast<double>(box.center.x) - halfX, cellSize);
    const auto minY = cellIndex(static_cast<double>(box.center.y) - halfY, cellSize);
    const auto minZ = cellIndex(static_cast<double>(box.center.z) - halfZ, cellSize);
    const auto maxX = cellIndex(static_cast<double>(box.center.x) + halfX, cellSize);
    const auto maxY = cellIndex(static_cast<double>(box.center.y) + halfY, cellSize);
    const auto maxZ = cellIndex(static_cast<double>(box.center.z) + halfZ, cellSize);
    if (!minX || !minY || !minZ || !maxX || !maxY || !maxZ) {
        return std::nullopt;
    }
    return CellRange{{*minX, *minY, *minZ}, {*maxX, *maxY, *maxZ}};
}

bool manageable(CellRange range)
{
    constexpr long double maximumCells = 4096.0L;
    const long double x = static_cast<long double>(range.maximum.x) - range.minimum.x + 1.0L;
    const long double y = static_cast<long double>(range.maximum.y) - range.minimum.y + 1.0L;
    const long double z = static_cast<long double>(range.maximum.z) - range.minimum.z + 1.0L;
    return x * y * z <= maximumCells;
}

} // namespace

BroadPhaseResult broadPhaseNaive(std::span<const Aabb> bounds)
{
    BroadPhaseResult result;
    for (std::size_t first = 0; first < bounds.size(); ++first) {
        for (std::size_t second = first + 1; second < bounds.size(); ++second) {
            ++result.testedPairs;
            if (intersects(bounds[first], bounds[second])) {
                result.pairs.push_back({first, second});
            }
        }
    }
    return result;
}

BroadPhaseResult broadPhaseGrid(std::span<const Aabb> bounds, float cellSize)
{
    if (!std::isfinite(cellSize) || cellSize <= 0.0f) {
        throw std::invalid_argument("Broad phase cell size must be positive and finite");
    }

    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> grid;
    for (std::size_t index = 0; index < bounds.size(); ++index) {
        const auto range = cellRange(bounds[index], cellSize);
        if (!range || !manageable(*range)) {
            return broadPhaseNaive(bounds);
        }
        for (std::int64_t x = range->minimum.x; x <= range->maximum.x; ++x) {
            for (std::int64_t y = range->minimum.y; y <= range->maximum.y; ++y) {
                for (std::int64_t z = range->minimum.z; z <= range->maximum.z; ++z) {
                    grid[{x, y, z}].push_back(index);
                }
            }
        }
    }

    BroadPhaseResult result;
    result.occupiedCells = grid.size();
    std::unordered_set<BroadPhasePair, PairHash> seen;
    for (const auto &[cell, indices] : grid) {
        for (std::size_t i = 0; i < indices.size(); ++i) {
            for (std::size_t j = i + 1; j < indices.size(); ++j) {
                const BroadPhasePair pair{indices[i], indices[j]};
                if (!seen.insert(pair).second) {
                    continue;
                }
                ++result.testedPairs;
                if (intersects(bounds[pair.first], bounds[pair.second])) {
                    result.pairs.push_back(pair);
                }
            }
        }
    }
    std::sort(result.pairs.begin(), result.pairs.end(), [](BroadPhasePair a, BroadPhasePair b) {
        return a.first < b.first || (a.first == b.first && a.second < b.second);
    });
    return result;
}

} // namespace rtype::physics
