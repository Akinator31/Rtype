// Tests of the story of the game in the client: the lore file, its layout on the crawl, and the CrawlSystem
// that scrolls it away towards the horizon.

#include "Lore/Crawl.hpp"
#include "Lore/CrawlSystem.hpp"
#include "Lore/LoreLayout.hpp"
#include "Lore/LoreText.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Engine/Engine.hpp"
#include "rtype/engine/Interface/Metrics.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace rtype::client;
using Catch::Approx;

namespace
{

/// Every character is 10 units wide at any size, a space included.
class FixedMetrics final : public rtype::engine::Metrics
{
public:
    [[nodiscard]] rtype::render::Vector2 measureText(rtype::render::FontId /*font*/, const std::string &text,
                                                     float size, float /*spacing*/) const override
    {
        return {10.0f * static_cast<float>(text.size()), size};
    }
    [[nodiscard]] rtype::render::Vector2 gameSize() const override { return {1920.0f, 1080.0f}; }
    [[nodiscard]] rtype::render::Rectangle visibleArea() const override
    {
        return {0.0f, 0.0f, 1920.0f, 1080.0f};
    }
    [[nodiscard]] rtype::render::Vector2 windowToGame(rtype::render::Vector2 position) const override
    {
        return position;
    }
};

/// Lines of 10 units per size unit, a column of 100 units: 10 characters per line.
LoreStyle createTestStyle()
{
    LoreStyle style;
    for (LoreLineStyle &kind : style.kinds) {
        kind.size = 10.0f;
    }
    style.width = 100.0f;
    style.lineHeight = 2.0f;
    return style;
}

std::vector<LoreLine> parse(const std::string &text)
{
    std::istringstream stream(text);
    return parseLore(stream);
}

} // namespace

TEST_CASE("The lore file gives the kind of each line by its prefix", "[client]")
{
    const std::vector<LoreLine> lines =
        parse("### YEAR 2126\n## CONQUEST\n# TITLE  \n\n  Humanity travels.\r\n");

    REQUIRE(lines.size() == 5);
    CHECK(lines[0].kind == LoreLineKind::Subheading);
    CHECK(lines[0].text == "YEAR 2126");
    CHECK(lines[1].kind == LoreLineKind::Heading);
    CHECK(lines[1].text == "CONQUEST");
    CHECK(lines[2].kind == LoreLineKind::Title);
    CHECK(lines[2].text == "TITLE");
    CHECK(lines[3].kind == LoreLineKind::Blank);
    CHECK(lines[4].kind == LoreLineKind::Paragraph);
    CHECK(lines[4].text == "Humanity travels.");
}

TEST_CASE("A lore file that cannot be read gives no line", "[client]")
{
    CHECK(readLoreFile("no/such/lore.txt").empty());
}

TEST_CASE("A paragraph is justified on the column but its last line", "[client]")
{
    const FixedMetrics metrics;
    // "aaaa bbb" fits in 10 characters, "cccc" does not fit after it: two lines
    const LoreLayout layout = layOutLore(parse("aaaa bbb cccc"), createTestStyle(), metrics);

    REQUIRE(layout.pieces.size() == 3);
    // The first line spreads over the whole column: from -50 to 50
    CHECK(layout.pieces[0].text == "aaaa");
    CHECK(layout.pieces[0].x == Approx(-50.0f + 20.0f));
    CHECK(layout.pieces[1].text == "bbb");
    CHECK(layout.pieces[1].x == Approx(50.0f - 15.0f));
    // The last line is on the left
    CHECK(layout.pieces[2].text == "cccc");
    CHECK(layout.pieces[2].x == Approx(-50.0f + 20.0f));
    // Lines of 20 units, each piece at the middle of its line
    CHECK(layout.pieces[0].distance == Approx(10.0f));
    CHECK(layout.pieces[2].distance == Approx(30.0f));
    CHECK(layout.length == Approx(40.0f));
}

TEST_CASE("A colon written after a space stays with the word before it", "[client]")
{
    const FixedMetrics metrics;
    const LoreLayout layout = layOutLore(parse("hostile : Bydoo"), createTestStyle(), metrics);

    REQUIRE(layout.pieces.size() == 2);
    CHECK(layout.pieces[0].text == "hostile :");
    CHECK(layout.pieces[1].text == "Bydoo");
}

TEST_CASE("Headings are centred whole, and empty lines make space", "[client]")
{
    const FixedMetrics metrics;
    const LoreLayout layout = layOutLore(parse("## THE WAR\n\n### 2126"), createTestStyle(), metrics);

    REQUIRE(layout.pieces.size() == 2);
    CHECK(layout.pieces[0].text == "THE WAR");
    CHECK(layout.pieces[0].x == Approx(0.0f));
    CHECK(layout.pieces[0].kind == LoreLineKind::Heading);
    CHECK(layout.pieces[1].kind == LoreLineKind::Subheading);
    CHECK(layout.pieces[1].distance == Approx(50.0f));
}

TEST_CASE("The crawl shrinks the text towards the horizon", "[client]")
{
    const Crawl crawl{.centreX = 960.0f, .bottom = 1080.0f, .depth = 1000.0f};

    const CrawlProjection atBottom = projectOnCrawl(crawl, 100.0f, 0.0f);
    CHECK(atBottom.position.x == Approx(1060.0f));
    CHECK(atBottom.position.y == Approx(1080.0f));
    CHECK(atBottom.scale == Approx(1.0f));
    CHECK(atBottom.opacity == Approx(1.0f));
    CHECK_FALSE(atBottom.hidden);

    Crawl further = crawl;
    further.scrolled = 1000.0f;
    // Half the size, half way to the horizon, and just starting to fade out
    const CrawlProjection halfway = projectOnCrawl(further, 100.0f, 0.0f);
    CHECK(halfway.scale == Approx(0.5f));
    CHECK(halfway.position.x == Approx(1010.0f));
    CHECK(halfway.position.y == Approx(580.0f));
    CHECK(halfway.opacity == Approx(1.0f));

    further.scrolled = 5000.0f;
    CHECK(projectOnCrawl(further, 0.0f, 0.0f).hidden);
    // Far below the bottom of the screen
    CHECK(projectOnCrawl(crawl, 0.0f, 900.0f).hidden);
}

TEST_CASE("The crawl scrolls its items, then fades out and ends", "[client]")
{
    rtype::engine::Engine engine;
    rtype::ecs::Ecs &ecs = engine.getEcs();
    ecs.registerSystem<CrawlSystem>(engine.getScenes());
    const rtype::engine::SceneId scene = engine.getScenes().open();
    bool ended = false;
    const rtype::ecs::entity_t crawl = ecs.createEntity();
    ecs.addComponent<rtype::engine::SceneMember>(crawl, scene);
    ecs.addComponent<Crawl>(crawl, Crawl{.bottom = 1000.0f,
                                         .depth = 1000.0f,
                                         .speed = 100.0f,
                                         .length = 0.0f,
                                         .onEnd = [&ended] { ended = true; }});
    const rtype::ecs::entity_t item = ecs.createEntity();
    ecs.addComponent<rtype::engine::SceneMember>(item, scene);
    ecs.addComponent<rtype::ecs::Position>(item);
    ecs.addComponent<rtype::engine::Scale>(item);
    ecs.addComponent<rtype::engine::Opacity>(item);
    ecs.addComponent<rtype::ecs::Hidden>(item);
    ecs.addComponent<CrawlItem>(item, crawl, 0.0f, 0.0f);

    engine.update(0.0f);
    CHECK_FALSE(ecs.hasComponent<rtype::ecs::Hidden>(item));
    CHECK(ecs.getComponent<rtype::ecs::Position>(item).y == Approx(1000.0f));

    engine.update(9.0f);
    CHECK(ecs.getComponent<rtype::engine::Scale>(item).value == Approx(1000.0f / 1900.0f));

    // 1000 units away: half its size, half way to the horizon. Its end fades out from then on
    engine.update(1.0f);
    CHECK(ecs.getComponent<rtype::engine::Scale>(item).value == Approx(0.5f));
    CHECK(ecs.getComponent<rtype::ecs::Position>(item).y == Approx(500.0f));
    CHECK(ecs.getComponent<rtype::engine::Opacity>(item).value == Approx(2.0f / 3.0f));

    engine.update(1.0f);
    CHECK_FALSE(ended);
    engine.update(1.0f);
    CHECK(ended);
    CHECK(ecs.hasComponent<rtype::ecs::Hidden>(item));
}
