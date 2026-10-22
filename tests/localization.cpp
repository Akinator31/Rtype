#include "rtype/localization/Localization.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/System/BoundTextSystem.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

struct CatalogFile {
    std::filesystem::path path;

    explicit CatalogFile(const std::string &content)
    {
        static std::atomic<unsigned> next = 0;
        path = std::filesystem::temp_directory_path() /
               ("rtype_localization_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                std::to_string(next++));
        std::ofstream(path) << content;
    }

    ~CatalogFile() { std::filesystem::remove(path); }
};

} // namespace

TEST_CASE("Localization switches languages at runtime and falls back to the default", "[localization]")
{
    CatalogFile english("menu.play=Play\nmenu.quit=Quit\n");
    CatalogFile french("menu.play=Jouer\nmenu.accent=Français\n");
    rtype::localization::Localization translations("en");
    translations.load("en", english.path);
    translations.load("fr", french.path);

    CHECK(translations.text("menu.play") == "Play");
    REQUIRE(translations.setLanguage("fr"));
    CHECK(translations.text("menu.play") == "Jouer");
    CHECK(translations.text("menu.accent") == "Français");
    CHECK(translations.text("menu.quit") == "Quit");
    CHECK(translations.text("missing.key") == "missing.key");
    CHECK_FALSE(translations.setLanguage("missing"));
    CHECK(translations.language() == "fr");
}

TEST_CASE("Localization rejects broken catalogs without replacing a loaded language", "[localization]")
{
    CatalogFile valid("menu.play=Play\n");
    CatalogFile duplicate("menu.play=Play\nmenu.play=Again\n");
    CatalogFile malformed("missing separator\n");
    rtype::localization::Localization translations("en");
    translations.load("en", valid.path);

    CHECK_THROWS(translations.load("en", duplicate.path));
    CHECK_THROWS(translations.load("en", malformed.path));
    CHECK_THROWS(translations.load("en", valid.path.string() + ".missing"));
    CHECK(translations.text("menu.play") == "Play");
}

TEST_CASE("Bound interface text follows a language change without reopening its scene", "[localization]")
{
    CatalogFile english("menu.play=Play\n");
    CatalogFile french("menu.play=Jouer\n");
    rtype::localization::Localization translations("en");
    translations.load("en", english.path);
    translations.load("fr", french.path);

    rtype::ecs::Ecs ecs;
    ecs.registerSystem<rtype::engine::BoundTextSystem>();
    const auto entity = ecs.createEntity();
    ecs.addComponent<rtype::engine::Text>(entity, "", rtype::render::FontId{0});
    ecs.addComponent<rtype::engine::BoundText>(entity,
                                               [&translations] { return translations.text("menu.play"); });
    ecs.update(0.0f);
    CHECK(ecs.getComponent<rtype::engine::Text>(entity).text == "Play");

    REQUIRE(translations.setLanguage("fr"));
    ecs.update(0.0f);
    CHECK(ecs.getComponent<rtype::engine::Text>(entity).text == "Jouer");
}
