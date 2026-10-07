// The colour filters run on the graphics card and are checked with screenshots; their names are tested here.

#include "rtype/render/ColorFilter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string_view>

using rtype::render::ColorFilter;
using rtype::render::colorFilterName;

TEST_CASE("Every colour filter has its own name", "[render][accessibility]")
{
    std::set<std::string_view> names;
    for (int filter = 0; filter <= static_cast<int>(ColorFilter::HighContrast); ++filter) {
        const std::string_view name = colorFilterName(static_cast<ColorFilter>(filter));
        REQUIRE_FALSE(name.empty());
        REQUIRE(name != "Unknown");
        names.insert(name);
    }
    REQUIRE(names.size() == 8);
}
