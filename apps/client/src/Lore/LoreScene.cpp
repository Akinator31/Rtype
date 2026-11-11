#include "Lore/LoreScene.hpp"

#include "Assets.hpp"
#include "Lore/Crawl.hpp"
#include "Lore/LoreLayout.hpp"
#include "Lore/LoreText.hpp"
#include "MainMenu/Layout.hpp"
#include "MainMenu/MainMenu.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Interface/InterfaceBuilder.hpp"
#include "rtype/engine/Scene/SceneAsset.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/render/Paths.hpp"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <vector>

namespace rtype::client
{

namespace
{

/// Size of the fonts once loaded, large enough for the texts at the bottom of the screen.
constexpr int loreFontSize = 96;

/// The horizon is this far above the bottom of the screen: a little above its top, so that the text fades
/// out in the upper part of the screen.
constexpr float crawlDepth = 1200.0f;
constexpr float crawlSpeed = 80.0f;
/// The text starts this far below the bottom of the screen, and comes in.
constexpr float crawlLead = 160.0f;

/// The texts are drawn over the sky.
constexpr int loreOrder = 1;

/// The music, loaded before the scene opens and unloaded once it has closed, which stops it.
const std::vector<engine::SceneAsset> LoreAssets{{engine::AssetType::Music, RTYPE_MUSIC_LORE}};

/// A text of the crawl, in each language: a path of the assets.
struct LoreFiles {
    const char *french;
    const char *english;
};

constexpr LoreFiles StoryFiles{RTYPE_LORE_FRENCH, RTYPE_LORE_ENGLISH};
constexpr LoreFiles EndingFiles{RTYPE_CAMPAIGN_ENDING_FRENCH, RTYPE_CAMPAIGN_ENDING_ENGLISH};

[[nodiscard]] std::filesystem::path getLorePath(const LoreFiles &files,
                                                const localization::Localization &translations)
{
    const char *file = translations.language() == "fr" ? files.french : files.english;
    return render::applicationDirectory() / "assets" / file;
}

[[nodiscard]] std::vector<LoreLine> readLore(const LoreFiles &files,
                                             const localization::Localization &translations)
{
    const std::filesystem::path path = getLorePath(files, translations);
    std::vector<LoreLine> lines = readLoreFile(path);
    if (lines.empty()) {
        const log::Logger logger("client");
        logger.warning("Cannot read the lore {}", path.string());
    }
    return lines;
}

/// The title in heavy capitals, the headings and the years in the font of the game, the paragraphs in the
/// font of the buttons.
[[nodiscard]] LoreStyle createLoreStyle(render::ResourceManager &resources,
                                        const engine::InterfaceStyle &style)
{
    const render::FontId display = resources.loadFont(RTYPE_FONT_DISPLAY, loreFontSize);
    const render::FontId text = resources.loadFont(RTYPE_FONT_BUTTON, loreFontSize);
    // In the order of LoreLineKind
    return {.kinds = {{
                {.font = style.font, .size = 104.0f, .spacing = 6.0f},
                {.font = display, .size = 64.0f, .spacing = 3.0f},
                {.font = display, .size = 38.0f, .spacing = 10.0f},
                {.font = text, .size = 50.0f, .spacing = 0.0f},
                {.font = text, .size = 50.0f, .spacing = 0.0f},
            }}};
}

void createLoreSky(ecs::Ecs &ecs, engine::SceneId scene, render::ResourceManager &resources)
{
    const ecs::entity_t sky = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(sky, scene);
    ecs.addComponent<engine::Backdrop>(
        sky, resources.loadTexture(RTYPE_IMAGE_STARFIELD, render::TextureFiltering::Smooth));
}

ecs::entity_t createCrawl(engine::Engine &engine, engine::SceneId scene, const engine::Metrics &metrics,
                          float length, const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    const render::Rectangle screen = metrics.visibleArea();
    const ecs::entity_t crawl = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(crawl, scene);
    ecs.addComponent<Crawl>(crawl, Crawl{.centreX = screen.x + (screen.width / 2.0f),
                                         .bottom = screen.y + screen.height,
                                         .depth = crawlDepth,
                                         .speed = crawlSpeed,
                                         .scrolled = -crawlLead,
                                         .length = length,
                                         .onEnd = close});
    return crawl;
}

void createLorePiece(ecs::Ecs &ecs, engine::SceneId scene, const LorePiece &piece, const LoreStyle &style,
                     ecs::entity_t crawl)
{
    const LoreLineStyle &look = style.kinds[static_cast<std::size_t>(piece.kind)];
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<ecs::Position>(entity);
    ecs.addComponent<engine::Drawable>(entity, engine::Layer::World, loreOrder);
    ecs.addComponent<engine::Text>(entity, piece.text, look.font, look.size, render::colors::White,
                                   look.spacing);
    ecs.addComponent<engine::Scale>(entity);
    ecs.addComponent<engine::Opacity>(entity);
    ecs.addComponent<ecs::Hidden>(entity);
    ecs.addComponent<CrawlItem>(entity, crawl, piece.x, piece.distance);
}

void playLoreMusic(audio::AudioPlayer &audio)
{
    try {
        audio.playMusic(audio.loadMusic(RTYPE_MUSIC_LORE), false);
    } catch (const std::runtime_error &error) {
        const log::Logger logger("client");
        logger.warning("Music of the lore not played: {}", error.what());
    }
}

/// The crawl of a text on the stars, with the music of the lore. `close` leaves it: at its end, with its
/// back button.
void buildLoreScene(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                    audio::AudioPlayer &audio, const engine::InterfaceStyle &style,
                    const engine::Metrics &metrics, std::vector<LoreLine> lines,
                    const std::function<void()> &close)
{
    ecs::Ecs &ecs = engine.getEcs();
    createLoreSky(ecs, scene, resources);
    const LoreStyle loreStyle = createLoreStyle(resources, style);
    const LoreLayout layout = layOutLore(lines, loreStyle, metrics);
    const ecs::entity_t crawl = createCrawl(engine, scene, metrics, layout.length, close);
    for (const LorePiece &piece : layout.pieces) {
        createLorePiece(ecs, scene, piece, loreStyle, crawl);
    }
    engine::InterfaceBuilder interface(ecs, scene, style);
    interface.backButton(close, {engine::AnchorPoint::TopLeft, screenMargin, screenMargin});
    playLoreMusic(audio);
}

} // namespace

void createLoreScene(engine::Engine &engine, render::ResourceManager &resources, audio::AudioPlayer &audio,
                     const engine::InterfaceStyle &style, const engine::Metrics &metrics,
                     const localization::Localization &translations)
{
    engine.createScene(
        LoreScene, {.assets = LoreAssets},
        [&resources, &audio, &style, &metrics, &translations](engine::Engine &target, engine::SceneId scene) {
            buildLoreScene(target, scene, resources, audio, style, metrics,
                           readLore(StoryFiles, translations), [&target] { target.popScene(); });
        });
}

void createCampaignEndingScene(engine::Engine &engine, render::ResourceManager &resources,
                               audio::AudioPlayer &audio, const engine::InterfaceStyle &style,
                               const engine::Metrics &metrics, const localization::Localization &translations)
{
    engine.createScene(
        CampaignEndingScene,
        {.onBack = [&engine] { engine.switchScene(MainMenuScene); }, .assets = LoreAssets},
        [&resources, &audio, &style, &metrics, &translations](engine::Engine &target, engine::SceneId scene) {
            buildLoreScene(target, scene, resources, audio, style, metrics,
                           readLore(EndingFiles, translations),
                           [&target] { target.switchScene(MainMenuScene); });
        });
}

} // namespace rtype::client
