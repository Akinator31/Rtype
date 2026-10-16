#include "rtype/physics/Geometry/BroadPhase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

using rtype::physics::Aabb;
using rtype::physics::broadPhaseGrid;
using rtype::physics::broadPhaseNaive;
using rtype::physics::BroadPhasePair;

TEST_CASE("Spatial grid finds exactly the naive overlapping pairs", "[physics][broadphase]")
{
    const std::vector<Aabb> bounds{
        {{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f}},     {{1.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f}},
        {{4.0f, 0.0f, 0.0f}, {3.0f, 0.5f, 0.0f}},     {{-8.0f, -8.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        {{-8.0f, -8.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},   {{0.0f, 0.0f, 2.0f}, {0.5f, 0.5f, 0.0f}},
        {{100.0f, 100.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
    };
    const auto reference = broadPhaseNaive(bounds);
    REQUIRE(reference.pairs == std::vector<BroadPhasePair>{{0, 1}, {1, 2}, {3, 4}});
    CHECK(reference.testedPairs == 21);

    for (const float cellSize : {0.5f, 1.0f, 2.0f, 10.0f}) {
        const auto grid = broadPhaseGrid(bounds, cellSize);
        CHECK(grid.pairs == reference.pairs);
        CHECK(grid.testedPairs <= reference.testedPairs);
        CHECK(grid.occupiedCells > 0);
    }
}

TEST_CASE("Spatial grid matches the naive version across a crowded scene", "[physics][broadphase]")
{
    std::vector<Aabb> bounds;
    for (int x = -8; x < 8; ++x) {
        for (int y = -6; y < 6; ++y) {
            bounds.push_back(
                {{static_cast<float>(x) * 3.0f, static_cast<float>(y) * 3.0f, 0.0f}, {0.75f, 0.75f, 0.0f}});
            if ((x + y) % 3 == 0) {
                bounds.push_back({{static_cast<float>(x) * 3.0f + 0.5f, static_cast<float>(y) * 3.0f, 0.0f},
                                  {0.75f, 0.75f, 0.0f}});
            }
        }
    }

    const auto reference = broadPhaseNaive(bounds);
    for (const float cellSize : {1.0f, 3.0f, 8.0f}) {
        const auto grid = broadPhaseGrid(bounds, cellSize);
        CHECK(grid.pairs == reference.pairs);
        CHECK(grid.testedPairs <= reference.testedPairs);
    }
}

TEST_CASE("Spatial grid reduces tested pairs for separated colliders", "[physics][broadphase]")
{
    std::vector<Aabb> bounds;
    for (int i = 0; i < 400; ++i) {
        bounds.push_back({{static_cast<float>(i) * 10.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f}});
    }

    const auto reference = broadPhaseNaive(bounds);
    const auto grid = broadPhaseGrid(bounds, 2.0f);
    CHECK(reference.testedPairs == 79800);
    CHECK(grid.testedPairs == 0);
    CHECK(grid.pairs == reference.pairs);
}

TEST_CASE("Spatial grid falls back for shapes spanning too many cells", "[physics][broadphase]")
{
    const std::vector<Aabb> bounds{
        {{}, {10000.0f, 10000.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.5f, 0.5f, 0.0f}},
    };
    const auto reference = broadPhaseNaive(bounds);
    const auto grid = broadPhaseGrid(bounds, 1.0f);
    CHECK(grid.pairs == reference.pairs);
    CHECK(grid.testedPairs == reference.testedPairs);
    CHECK(grid.occupiedCells == 0);
}

TEST_CASE("Spatial grid rejects invalid cell sizes", "[physics][broadphase]")
{
    const std::vector<Aabb> bounds;
    CHECK_THROWS_AS(broadPhaseGrid(bounds, 0.0f), std::invalid_argument);
    CHECK_THROWS_AS(broadPhaseGrid(bounds, -1.0f), std::invalid_argument);
    CHECK_THROWS_AS(broadPhaseGrid(bounds, std::numeric_limits<float>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(broadPhaseGrid(bounds, std::numeric_limits<float>::quiet_NaN()), std::invalid_argument);
}
