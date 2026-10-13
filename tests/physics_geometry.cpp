#include "rtype/physics/Geometry/Intersections.hpp"

#include <catch2/catch_test_macros.hpp>

using rtype::physics::Aabb;
using rtype::physics::contains;
using rtype::physics::intersects;
using rtype::physics::penetration;
using rtype::physics::Ray;
using rtype::physics::raycast;
using rtype::physics::Sphere;
using rtype::physics::Vec3;

TEST_CASE("AABB intersections include touching and zero-size boxes", "[physics][geometry]")
{
    const Aabb box{{}, {2.0f, 2.0f, 0.0f}};
    CHECK(intersects(box, Aabb{{4.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 0.0f}}));
    CHECK_FALSE(intersects(box, Aabb{{4.1f, 0.0f, 0.0f}, {2.0f, 2.0f, 0.0f}}));
    CHECK_FALSE(intersects(box, Aabb{{0.0f, 0.0f, 1.0f}, {2.0f, 2.0f, 0.0f}}));
    CHECK(intersects(Aabb{}, Aabb{}));
    CHECK(contains(box, {2.0f, 2.0f, 0.0f}));
    CHECK_FALSE(contains(box, {2.0f, 2.0f, 0.1f}));
    CHECK(contains(Aabb{}, Vec3{}));
}

TEST_CASE("Sphere intersections and point containment include tangency", "[physics][geometry]")
{
    const Sphere sphere{{}, 2.0f};
    CHECK(intersects(sphere, Sphere{{4.0f, 0.0f, 0.0f}, 2.0f}));
    CHECK_FALSE(intersects(sphere, Sphere{{4.1f, 0.0f, 0.0f}, 2.0f}));
    CHECK(intersects(Sphere{}, Sphere{}));
    CHECK(contains(sphere, {0.0f, 0.0f, 2.0f}));
    CHECK_FALSE(contains(sphere, {0.0f, 0.0f, 2.1f}));
    CHECK(contains(Sphere{}, Vec3{}));
}

TEST_CASE("AABB sphere intersections handle corners and either argument order", "[physics][geometry]")
{
    const Aabb box{{}, {1.0f, 1.0f, 0.0f}};
    const Sphere tangent{{4.0f, 5.0f, 0.0f}, 5.0f};
    CHECK(intersects(box, tangent));
    CHECK(intersects(tangent, box));
    CHECK_FALSE(intersects(box, Sphere{{4.0f, 5.0f, 0.0f}, 4.0f}));
    CHECK(intersects(Aabb{}, Sphere{}));
}

TEST_CASE("Raycast finds the first AABB hit and handles parallel or inside rays", "[physics][geometry]")
{
    const Aabb box{{}, {1.0f, 1.0f, 0.0f}};
    const auto hit = raycast(Ray{{-5.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, box);
    REQUIRE(hit);
    CHECK(hit->t == 2.0f);
    CHECK(hit->point.x == -1.0f);
    CHECK(hit->normal.x == -1.0f);
    CHECK_FALSE(raycast(Ray{{-5.0f, 2.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, box));
    CHECK_FALSE(raycast(Ray{{-5.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}}, box));
    const auto inside = raycast(Ray{{}, {}}, box);
    REQUIRE(inside);
    CHECK(inside->t == 0.0f);
    CHECK(raycast(Ray{{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, box));
}

TEST_CASE("Raycast finds sphere entry and tangency", "[physics][geometry]")
{
    const Sphere sphere{{}, 1.0f};
    const auto hit = raycast(Ray{{-5.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, sphere);
    REQUIRE(hit);
    CHECK(hit->t == 2.0f);
    CHECK(hit->point.x == -1.0f);
    CHECK(hit->normal.x == -1.0f);
    const auto tangent = raycast(Ray{{-5.0f, 1.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, sphere);
    REQUIRE(tangent);
    CHECK(tangent->t == 2.5f);
    CHECK_FALSE(raycast(Ray{{-5.0f, 2.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, sphere));
    CHECK_FALSE(raycast(Ray{{-5.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}}, sphere));
    const auto inside = raycast(Ray{{}, {}}, sphere);
    REQUIRE(inside);
    CHECK(inside->t == 0.0f);
}

TEST_CASE("Raycast also handles depth in 3D", "[physics][geometry]")
{
    const Ray ray{{0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 2.0f}};
    const auto boxHit = raycast(ray, Aabb{{}, {1.0f, 1.0f, 1.0f}});
    REQUIRE(boxHit);
    CHECK(boxHit->t == 2.0f);
    CHECK(boxHit->point.z == -1.0f);
    CHECK(boxHit->normal.z == -1.0f);

    const auto sphereHit = raycast(ray, Sphere{{}, 1.0f});
    REQUIRE(sphereHit);
    CHECK(sphereHit->t == 2.0f);
    CHECK(sphereHit->normal.z == -1.0f);
}

TEST_CASE("Raycast can hit zero-size shapes", "[physics][geometry]")
{
    const Ray ray{{-2.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    const auto boxHit = raycast(ray, Aabb{});
    REQUIRE(boxHit);
    CHECK(boxHit->t == 2.0f);
    CHECK(boxHit->point.x == 0.0f);

    const auto sphereHit = raycast(ray, Sphere{});
    REQUIRE(sphereHit);
    CHECK(sphereHit->t == 2.0f);
    CHECK(sphereHit->point.x == 0.0f);
    CHECK_FALSE(raycast(Ray{{-2.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}, Aabb{}));
    CHECK_FALSE(raycast(Ray{{-2.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}, Sphere{}));
}

TEST_CASE("AABB penetration chooses the shallowest useful axis", "[physics][geometry]")
{
    const Aabb box{{}, {2.0f, 2.0f, 0.0f}};
    const auto planar = penetration(box, Aabb{{3.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 0.0f}});
    REQUIRE(planar);
    CHECK(planar->normal.x == 1.0f);
    CHECK(planar->normal.z == 0.0f);
    CHECK(planar->depth == 1.0f);
    const auto touch = penetration(box, Aabb{{4.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 0.0f}});
    REQUIRE(touch);
    CHECK(touch->depth == 0.0f);
    CHECK_FALSE(penetration(box, Aabb{{5.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 0.0f}}));
    const auto spatial =
        penetration(Aabb{{}, {2.0f, 2.0f, 2.0f}}, Aabb{{0.0f, 0.0f, 3.0f}, {2.0f, 2.0f, 2.0f}});
    REQUIRE(spatial);
    CHECK(spatial->normal.z == 1.0f);
    CHECK(spatial->depth == 1.0f);
}

TEST_CASE("Sphere penetration returns direction and overlap depth", "[physics][geometry]")
{
    const auto hit = penetration(Sphere{{}, 2.0f}, Sphere{{3.0f, 0.0f, 0.0f}, 2.0f});
    REQUIRE(hit);
    CHECK(hit->normal.x == 1.0f);
    CHECK(hit->depth == 1.0f);
    const auto coincident = penetration(Sphere{}, Sphere{});
    REQUIRE(coincident);
    CHECK(coincident->normal.x == 1.0f);
    CHECK(coincident->depth == 0.0f);
    CHECK_FALSE(penetration(Sphere{{}, 1.0f}, Sphere{{3.0f, 0.0f, 0.0f}, 1.0f}));
}

TEST_CASE("AABB sphere penetration handles outside and inside centers", "[physics][geometry]")
{
    const Aabb box{{}, {2.0f, 2.0f, 0.0f}};
    const auto outside = penetration(box, Sphere{{3.0f, 0.0f, 0.0f}, 2.0f});
    REQUIRE(outside);
    CHECK(outside->normal.x == 1.0f);
    CHECK(outside->depth == 1.0f);
    const auto reversed = penetration(Sphere{{3.0f, 0.0f, 0.0f}, 2.0f}, box);
    REQUIRE(reversed);
    CHECK(reversed->normal.x == -1.0f);
    CHECK(reversed->depth == 1.0f);
    const auto inside = penetration(box, Sphere{{1.5f, 0.0f, 0.0f}, 1.0f});
    REQUIRE(inside);
    CHECK(inside->normal.x == 1.0f);
    CHECK(inside->normal.z == 0.0f);
    CHECK(inside->depth == 1.5f);
    CHECK_FALSE(penetration(box, Sphere{{5.0f, 0.0f, 0.0f}, 1.0f}));
}
