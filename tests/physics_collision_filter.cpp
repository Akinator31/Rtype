#include "rtype/ecs/Ecs.hpp"
#include "rtype/physics/Physics.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using rtype::ecs::Ecs;
using rtype::ecs::entity_t;
using rtype::physics::CollisionFilter;
using rtype::physics::CollisionLayer;
using rtype::physics::CollisionMask;
using rtype::physics::canCollide;

namespace
{

CollisionFilter filterFor(Ecs &ecs, entity_t entity)
{
    auto &layers = ecs.cache<CollisionLayer>();
    auto &masks = ecs.cache<CollisionMask>();
    return {layers.has(entity) ? &layers.get(entity) : nullptr,
            masks.has(entity) ? &masks.get(entity) : nullptr};
}

}

TEST_CASE("Collision filters require both entities to accept the other layer", "[physics][filter]")
{
    constexpr std::uint32_t player = 1u << 0;
    constexpr std::uint32_t shot = 1u << 1;
    constexpr std::uint32_t enemy = 1u << 2;

    const CollisionLayer playerLayer{player};
    const CollisionLayer shotLayer{shot};
    const CollisionLayer enemyLayer{enemy};
    const CollisionMask playerMask{enemy};
    const CollisionMask shotMask{enemy};
    const CollisionMask enemyMask{player | shot};

    CHECK(canCollide({&shotLayer, &shotMask}, {&enemyLayer, &enemyMask}));
    CHECK(canCollide({&enemyLayer, &enemyMask}, {&shotLayer, &shotMask}));
    CHECK(canCollide({&playerLayer, &playerMask}, {&enemyLayer, &enemyMask}));
    CHECK_FALSE(canCollide({&playerLayer, &playerMask}, {&shotLayer, &shotMask}));
    CHECK_FALSE(canCollide({&shotLayer, &shotMask}, {&playerLayer, &playerMask}));

    const CollisionMask enemyOnly{enemy};
    CHECK_FALSE(canCollide({&shotLayer, &shotMask}, {&enemyLayer, &enemyOnly}));
}

TEST_CASE("Missing collision components accept every layer", "[physics][filter]")
{
    Ecs ecs;
    const entity_t unconfigured = ecs.createEntity();
    const entity_t configured = ecs.createEntity();
    ecs.addComponent<CollisionLayer>(configured, 1u << 2);
    ecs.addComponent<CollisionMask>(configured, 1u << 0);

    CHECK(canCollide(filterFor(ecs, unconfigured), filterFor(ecs, configured)));
    CHECK(canCollide(filterFor(ecs, configured), filterFor(ecs, unconfigured)));

    const entity_t noLayer = ecs.createEntity();
    ecs.addComponent<CollisionMask>(noLayer, 1u << 2);
    CHECK(canCollide(filterFor(ecs, noLayer), filterFor(ecs, configured)));

    const entity_t noMask = ecs.createEntity();
    ecs.addComponent<CollisionLayer>(noMask, 1u << 0);
    CHECK(canCollide(filterFor(ecs, noMask), filterFor(ecs, configured)));
}

TEST_CASE("Empty masks and layers reject collision pairs", "[physics][filter]")
{
    const CollisionLayer layer{1u};
    const CollisionLayer noLayer{0u};
    const CollisionMask mask{1u};
    const CollisionMask noMask{0u};

    CHECK_FALSE(canCollide({&layer, &noMask}, {&layer, &mask}));
    CHECK_FALSE(canCollide({&layer, &mask}, {&layer, &noMask}));
    CHECK_FALSE(canCollide({&noLayer, &mask}, {&layer, &mask}));
    CHECK_FALSE(canCollide({&layer, &mask}, {&noLayer, &mask}));
}
