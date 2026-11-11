#include "Campaign/CampaignScene.hpp"

#include "Assets.hpp"
#include "Campaign/Transmission.hpp"
#include "Lore/LoreText.hpp"
#include "MainMenu/Layout.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Scene/SceneAsset.hpp"
#include "rtype/log/Logger.hpp"
#include "rtype/render/Paths.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rtype::client
{

namespace
{

using engine::AssetType;

/// The sounds and the hum of the transmission, loaded before the scene opens and unloaded once it has
/// closed, which stops the hum.
const std::vector<engine::SceneAsset> CampaignAssets{
    {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_STATIC},  {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_BEEP},
    {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_CONNECT}, {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_TYPE},
    {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_LINE},    {AssetType::Sound, RTYPE_SOUND_TRANSMISSION_END},
    {AssetType::Music, RTYPE_MUSIC_TRANSMISSION_HUM},
};

/// Size of the fonts once loaded, large enough for the largest text.
constexpr int transmissionFontSize = 64;

/// The look of a kind of text of the transmission.
struct TextLook {
    render::FontId font;
    float size = 36.0f;
    float spacing = 0.0f;
    render::Color color = render::colors::White;
    /// Characters written per second.
    float speed = 40.0f;
};

/// The looks of the transmission: amber for the radio, cyan for the facts, a pale white for the message.
struct TransmissionLooks {
    TextLook signal;
    TextLook channel;
    TextLook title;
    TextLook fact;
    TextLook message;
    TextLook over;
    TextLook hint;
};

constexpr render::Color radioColor{255, 176, 64, 255};
constexpr render::Color channelColor{190, 135, 70, 255};
constexpr render::Color factColor{120, 220, 255, 255};
constexpr render::Color messageColor{225, 232, 240, 255};
constexpr render::Color hintColor{170, 180, 190, 255};

/// Width of the column of text at most, and the height of a line as a factor of the size of its text.
constexpr float columnWidth = 1400.0f;
constexpr float lineHeight = 1.45f;
/// Space under the signal and the name of the channel, under the name of the mission, between the facts and
/// the message, and above the end of the transmission.
constexpr float signalGap = 60.0f;
constexpr float titleGap = 24.0f;
constexpr float blankGap = 36.0f;
constexpr float overGap = 60.0f;

/// The black screen, then the texts over it.
constexpr int veilOrder = 0;
constexpr int textOrder = 1;

[[nodiscard]] TransmissionLooks createLooks(render::ResourceManager &resources)
{
    const render::FontId display = resources.loadFont(RTYPE_FONT_DISPLAY, transmissionFontSize);
    const render::FontId text = resources.loadFont(RTYPE_FONT_BUTTON, transmissionFontSize);
    return {.signal = {.font = display, .size = 40.0f, .spacing = 10.0f, .color = radioColor},
            .channel = {.font = text, .size = 24.0f, .spacing = 4.0f, .color = channelColor},
            .title = {.font = display, .size = 58.0f, .spacing = 4.0f, .speed = 20.0f},
            .fact = {.font = text, .size = 36.0f, .spacing = 1.0f, .color = factColor, .speed = 42.0f},
            .message = {.font = text, .size = 36.0f, .color = messageColor, .speed = 38.0f},
            .over = {.font = display, .size = 30.0f, .spacing = 10.0f, .color = radioColor},
            .hint = {.font = text, .size = 26.0f, .spacing = 2.0f, .color = hintColor}};
}

[[nodiscard]] std::vector<LoreLine> readBriefing(const CampaignMission &mission,
                                                 const localization::Localization &translations)
{
    const char *file = translations.language() == "fr" ? mission.briefingFrench : mission.briefingEnglish;
    const std::filesystem::path path = render::applicationDirectory() / "assets" / file;
    std::vector<LoreLine> lines = readLoreFile(path);
    if (lines.empty()) {
        const log::Logger logger("client");
        logger.warning("Cannot read the briefing of the campaign {}", path.string());
    }
    return lines;
}

/// A line of the transmission to place: its text, its look, and the space above it.
struct PlannedLine {
    std::string text;
    const TextLook *look = nullptr;
    float gap = 0.0f;
};

/// The lines of the briefing, cut to the width of the column.
[[nodiscard]] std::vector<PlannedLine> planBriefing(const std::vector<LoreLine> &briefing,
                                                    const TransmissionLooks &looks,
                                                    const engine::Metrics &metrics, float width)
{
    std::vector<PlannedLine> planned;
    bool message = false;
    float gap = 0.0f;
    for (const LoreLine &line : briefing) {
        if (line.kind == LoreLineKind::Blank) {
            message = message || !planned.empty();
            gap += blankGap;
            continue;
        }
        const bool title = line.kind == LoreLineKind::Title;
        const TextLook &look = title ? looks.title : (message ? looks.message : looks.fact);
        // Room for the cursor at the end of the line
        const auto measure = [&metrics, &look](const std::string &text) {
            return metrics.measureText(look.font, text + '_', look.size, look.spacing).x;
        };
        for (std::string &part : wrapWords(line.text, width, measure)) {
            planned.push_back({std::move(part), &look, std::exchange(gap, 0.0f)});
        }
        if (title) {
            gap += titleGap;
        }
    }
    return planned;
}

[[nodiscard]] float lineSpace(const TextLook &look)
{
    return look.size * lineHeight;
}

/// A text of the transmission, its top left corner `x` and `y` game units from the top left of the screen,
/// so that it grows to the right as it is written.
ecs::entity_t createLine(ecs::Ecs &ecs, engine::SceneId scene, const TextLook &look, std::string text,
                         float x, float y, engine::AnchorPoint point = engine::AnchorPoint::TopLeft)
{
    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<engine::Anchor>(entity, point, x, y);
    ecs.addComponent<ecs::Position>(entity);
    ecs.addComponent<engine::Drawable>(entity, engine::Layer::Interface, textOrder);
    ecs.addComponent<engine::Text>(entity, std::move(text), look.font, look.size, look.color, look.spacing);
    ecs.addComponent<engine::Opacity>(entity);
    return entity;
}

void createVeil(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics)
{
    const render::Rectangle area = metrics.visibleArea();
    const ecs::entity_t veil = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(veil, scene);
    ecs.addComponent<engine::Anchor>(veil, engine::AnchorPoint::Center, 0.0f, 0.0f);
    ecs.addComponent<ecs::Position>(veil);
    ecs.addComponent<engine::Drawable>(veil, engine::Layer::Interface, veilOrder);
    ecs.addComponent<engine::Box>(veil, area.width, area.height, render::colors::Black);
}

[[nodiscard]] TransmissionSounds loadSounds(audio::AudioPlayer &audio)
{
    return {.statics = audio.loadSound(RTYPE_SOUND_TRANSMISSION_STATIC),
            .beep = audio.loadSound(RTYPE_SOUND_TRANSMISSION_BEEP),
            .connect = audio.loadSound(RTYPE_SOUND_TRANSMISSION_CONNECT),
            .type = audio.loadSound(RTYPE_SOUND_TRANSMISSION_TYPE),
            .line = audio.loadSound(RTYPE_SOUND_TRANSMISSION_LINE),
            .end = audio.loadSound(RTYPE_SOUND_TRANSMISSION_END)};
}

void playHum(audio::AudioPlayer &audio)
{
    try {
        audio.playMusic(audio.loadMusic(RTYPE_MUSIC_TRANSMISSION_HUM), true);
    } catch (const std::runtime_error &error) {
        const log::Logger logger("client");
        logger.warning("Hum of the transmission not played: {}", error.what());
    }
}

void buildCampaignScene(engine::Engine &engine, engine::SceneId scene, render::ResourceManager &resources,
                        audio::AudioPlayer &audio, const engine::Metrics &metrics,
                        const localization::Localization &translations, Campaign &campaign)
{
    const CampaignMission &mission = campaign.brief();
    ecs::Ecs &ecs = engine.getEcs();
    createVeil(ecs, scene, metrics);
    const TransmissionLooks looks = createLooks(resources);
    const render::Rectangle area = metrics.visibleArea();
    const float width = std::min(columnWidth, area.width - (2.0f * screenMargin));
    const float left = (area.width - width) / 2.0f;
    const std::vector<PlannedLine> planned =
        planBriefing(readBriefing(mission, translations), looks, metrics, width);

    // The whole transmission is centred on the screen
    float height =
        lineSpace(looks.signal) + lineSpace(looks.channel) + signalGap + overGap + lineSpace(looks.over);
    for (const PlannedLine &line : planned) {
        height += line.gap + lineSpace(*line.look);
    }
    float y = std::max(screenMargin, (area.height - height) / 2.0f);

    Transmission transmission;
    transmission.signal =
        createLine(ecs, scene, looks.signal, translations.text("campaign.incoming"), left, y);
    y += lineSpace(looks.signal);
    transmission.channel =
        createLine(ecs, scene, looks.channel, translations.text("campaign.channel"), left, y);
    y += lineSpace(looks.channel) + signalGap;
    for (const PlannedLine &line : planned) {
        y += line.gap;
        const ecs::entity_t entity = createLine(ecs, scene, *line.look, std::string(), left, y);
        transmission.lines.push_back({entity, line.text, line.look->speed});
        y += lineSpace(*line.look);
    }
    transmission.over =
        createLine(ecs, scene, looks.over, translations.text("campaign.over"), left, y + overGap);
    transmission.hint = createLine(ecs, scene, looks.hint, translations.text("campaign.skip"), -screenMargin,
                                   -screenMargin, engine::AnchorPoint::BottomRight);
    for (const ecs::entity_t hidden :
         {transmission.signal, transmission.channel, transmission.over, transmission.hint}) {
        ecs.addComponent<ecs::Hidden>(hidden);
    }
    transmission.skipHint = translations.text("campaign.skip");
    transmission.continueHint = translations.text("campaign.continue");
    transmission.sounds = loadSounds(audio);
    // The game of the mission replaces every scene
    transmission.onEnd = [&campaign] { campaign.play(); };

    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<Transmission>(entity, std::move(transmission));
    playHum(audio);
}

} // namespace

void createCampaignScene(engine::Engine &engine, render::ResourceManager &resources,
                         audio::AudioPlayer &audio, const engine::Metrics &metrics,
                         const localization::Localization &translations, Campaign &campaign)
{
    engine.createScene(CampaignScene, {.onBack = [&campaign] { campaign.leave(); }, .assets = CampaignAssets},
                       [&resources, &audio, &metrics, &translations, &campaign](engine::Engine &target,
                                                                                engine::SceneId scene) {
                           buildCampaignScene(target, scene, resources, audio, metrics, translations,
                                              campaign);
                       });
}

} // namespace rtype::client
