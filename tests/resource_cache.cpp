// The ResourceCache does not depend on raylib: these tests use fake resources, without a window.

#include "rtype/core/Handle.hpp"
#include "rtype/core/ResourceCache.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using FakeId = rtype::core::Handle<struct FakeTag>;
// Move-only like the real resources, which own graphics memory
using FakeResource = std::unique_ptr<std::string>;
using FakeCache = rtype::core::ResourceCache<FakeResource, FakeId>;

FakeResource makeResource(const std::string &content)
{
    return std::make_unique<std::string>(content);
}

} // namespace

TEST_CASE("A loaded resource can be retrieved with its handle", "[render][resources]")
{
    FakeCache cache;

    const FakeId id = cache.load("player.png", [] { return makeResource("player"); });

    REQUIRE(*cache.get(id) == "player");
}

TEST_CASE("A key is only loaded once", "[render][resources]")
{
    FakeCache cache;
    int loads = 0;
    const auto loader = [&] {
        ++loads;
        return makeResource("player");
    };

    const FakeId first = cache.load("player.png", loader);
    const FakeId second = cache.load("player.png", loader);

    REQUIRE(first == second);
    REQUIRE(loads == 1);
    REQUIRE(cache.size() == 1);
}

TEST_CASE("Different keys get different handles", "[render][resources]")
{
    FakeCache cache;

    const FakeId player = cache.load("player.png", [] { return makeResource("player"); });
    const FakeId enemy = cache.load("enemy.png", [] { return makeResource("enemy"); });

    REQUIRE_FALSE(player == enemy);
    REQUIRE(*cache.get(player) == "player");
    REQUIRE(*cache.get(enemy) == "enemy");
}

TEST_CASE("A failed load leaves the cache unchanged", "[render][resources]")
{
    FakeCache cache;

    REQUIRE_THROWS_AS(
        cache.load("missing.png", []() -> FakeResource { throw std::runtime_error("missing"); }),
        std::runtime_error);
    REQUIRE(cache.size() == 0);

    // The key was not recorded: the next load tries again
    const FakeId id = cache.load("missing.png", [] { return makeResource("found"); });
    REQUIRE(*cache.get(id) == "found");
}

TEST_CASE("A handle that does not come from the cache is rejected", "[render][resources]")
{
    FakeCache cache;
    cache.load("player.png", [] { return makeResource("player"); });

    REQUIRE_THROWS_AS(cache.get(FakeId{42}), std::out_of_range);
}

TEST_CASE("Unloaded resources free their key without reusing a stale handle", "[render][resources]")
{
    FakeCache cache;
    const FakeId old = cache.load("ship.glb", [] { return makeResource("old"); });
    REQUIRE(cache.findId("ship.glb") == old);
    REQUIRE(cache.unload("ship.glb"));
    CHECK(cache.size() == 0);
    CHECK_FALSE(cache.findId("ship.glb"));
    CHECK_THROWS_AS(cache.get(old), std::out_of_range);

    const FakeId fresh = cache.load("ship.glb", [] { return makeResource("fresh"); });
    CHECK_FALSE(fresh == old);
    CHECK(*cache.get(fresh) == "fresh");
    CHECK_FALSE(cache.unload("missing.glb"));
}
