#include "rtype/render/Paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("The application directory is an existing absolute directory", "[render][paths]")
{
    const std::filesystem::path directory = rtype::render::applicationDirectory();

    REQUIRE(directory.is_absolute());
    REQUIRE(std::filesystem::is_directory(directory));
}
