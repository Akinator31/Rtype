#include "rtype/core/AssetFile.hpp"
#include "rtype/render/FontCharacters.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

namespace
{

const std::filesystem::path fonts = RTYPE_TEST_FONTS;

} // namespace

TEST_CASE("A font keeps only the wanted characters it can draw", "[render][font]")
{
    // Orbitron has the accented letters of French, but neither the guillemets nor the leftwards arrow
    const std::vector<unsigned char> orbitron = rtype::core::readAssetFile(fonts / "Orbitron-Medium.ttf");
    const std::vector<int> wanted{'A', 0xE9, 0xC8, 0xAB, 'z', 0x2190};

    CHECK(rtype::render::filterCharactersInFont(orbitron, wanted) == std::vector<int>{'A', 0xE9, 0xC8, 'z'});
}

TEST_CASE("A font that has every wanted character keeps them all, in order", "[render][font]")
{
    const std::vector<unsigned char> inter = rtype::core::readAssetFile(fonts / "Inter-SemiBold.ttf");
    const std::vector<int> wanted{0x2190, 'A', 0xAB, 0xE9, 0x153};

    CHECK(rtype::render::filterCharactersInFont(inter, wanted) == wanted);
}

TEST_CASE("Characters asked of a file that is not a font are all kept", "[render][font]")
{
    const std::vector<unsigned char> garbage{'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'n', 't'};
    const std::vector<int> wanted{'A', 0xE9};

    CHECK(rtype::render::filterCharactersInFont(garbage, wanted) == wanted);
    CHECK(rtype::render::filterCharactersInFont({}, wanted) == wanted);
}
