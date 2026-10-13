#include "rtype/shaders/ColorFilter.hpp"
#include "rtype/shaders/Lighting.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace
{

// Compiling the shaders needs a graphics context, which the tests do not have: they only check that each
// source is a complete GLSL 3.30 program
bool isProgram(std::string_view source)
{
    return source.find("#version 330") != std::string_view::npos &&
           source.find("void main()") != std::string_view::npos;
}

} // namespace

TEST_CASE("Shader sources are complete GLSL programs", "[shaders]")
{
    CHECK(isProgram(rtype::shaders::colorFilterFragment));
    CHECK(isProgram(rtype::shaders::lightingVertex));
    CHECK(isProgram(rtype::shaders::lightingFragment));
}

TEST_CASE("Shader sources declare the uniforms set by the renderer", "[shaders]")
{
    const std::string_view filter = rtype::shaders::colorFilterFragment;
    CHECK(filter.find("uniform int mode;") != std::string_view::npos);

    const std::string_view lighting = rtype::shaders::lightingFragment;
    for (const std::string_view uniform : {"uniform vec3 lightDirection;", "uniform vec3 lightColor;",
                                           "uniform vec3 ambientColor;", "uniform int useNormalMap;"}) {
        CHECK(lighting.find(uniform) != std::string_view::npos);
    }
}
