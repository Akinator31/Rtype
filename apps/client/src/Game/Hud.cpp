#include "Game/Hud.hpp"

#include "Game/Radar.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/game/Constants.hpp"

#include <array>
#include <string>
#include <utility>

namespace rtype::client
{

namespace
{

// The sizes are those of the Figma design, 1440 wide, scaled to the 1920 of the game: by 4/3 (ScreenHud)

/// Over the world of the game, under the death screen and the toasts; the shadows under the rest.
constexpr int ShadowOrder = 0;
constexpr int HudOrder = 1;

constexpr float TopShadowHeight = 173.3f;
constexpr float BottomShadowHeight = 200.0f;
constexpr render::Color TopShadow{0, 0, 0, 153};
constexpr render::Color BottomShadow{0, 0, 0, 166};
constexpr render::Color Clear{0, 0, 0, 0};

/// The pictures of the lives, the whole texture.
constexpr render::Rectangle LifeSource{0.0f, 0.0f, 120.0f, 48.0f};

constexpr render::Color PilotColor{138, 144, 151, 255};
constexpr render::Color HealthColor{80, 220, 100, 255};

constexpr render::Color AllySlotColor{107, 113, 119, 255};
constexpr render::Color AllyNameColor{138, 144, 151, 255};
constexpr render::Color AllyScoreColor{201, 205, 210, 255};
/// The experience of the local player, online.
constexpr render::Color XpColor{255, 214, 102, 255};

/// The icons of the voice: a square of the texture, drawn at the size of the text beside it.
constexpr render::Rectangle VoiceIconSource{0.0f, 0.0f, 64.0f, 64.0f};

constexpr render::Color PingColor{201, 205, 210, 255};

/// The alert of a boss, and the light of a game online, on the screens of the cockpit.
constexpr render::Color BossColor{232, 114, 110, 255};
constexpr render::Color OnlineLight{111, 227, 240, 255};

/// The radar of the hologram: its frame and title in its light, and its grid, faint.
constexpr render::Color RadarLight{111, 227, 240, 160};
constexpr render::Color RadarGrid{111, 227, 240, 40};
constexpr float RadarOutline = 2.0f;
/// Three columns and two rows inside the frame.
constexpr std::size_t RadarGridLines = 5;

/// An entity of the HUD, placed by the HudSystem from a point of the screen, or of its screen in the cockpit.
ecs::entity_t createPart(ecs::Ecs &ecs, engine::SceneId scene, int order, engine::AnchorPoint point,
                         engine::Surface surface)
{
    const ecs::entity_t part = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(part, scene);
    ecs.addComponent<engine::Anchor>(part, point);
    ecs.addComponent<ecs::Position>(part);
    ecs.addComponent<engine::Drawable>(part, engine::Layer::Interface, order);
    ecs.addComponent<engine::OnSurface>(part, surface);
    return part;
}

ecs::entity_t createText(ecs::Ecs &ecs, engine::SceneId scene, engine::AnchorPoint point,
                         engine::Surface surface, render::FontId font, float size, render::Color color,
                         float spacing)
{
    const ecs::entity_t text = createPart(ecs, scene, HudOrder, point, surface);
    ecs.addComponent<engine::Text>(text, std::string(), font, size, color, spacing);
    return text;
}

/// The icon of the voice of a pilot, hidden until the HudSystem shows it.
ecs::entity_t createVoiceIcon(ecs::Ecs &ecs, engine::SceneId scene, engine::AnchorPoint point,
                              engine::Surface surface, const HudLook &look, float size)
{
    const ecs::entity_t icon = createPart(ecs, scene, HudOrder, point, surface);
    ecs.addComponent<engine::Sprite>(icon, look.voice[0], VoiceIconSource, size, size);
    ecs.addComponent<engine::Opacity>(icon, 0.0f);
    return icon;
}

/// The shadows belong to the screen: hidden in the cockpit, whose screens are dark.
ecs::entity_t createShadow(ecs::Ecs &ecs, engine::SceneId scene, engine::AnchorPoint point, float height,
                           render::Color top, render::Color bottom)
{
    const ecs::entity_t shadow = createPart(ecs, scene, ShadowOrder, point, engine::Surface::Board);
    ecs.addComponent<engine::Box>(shadow, engine::Box{.height = height, .color = top, .bottom = bottom});
    ecs.addComponent<engine::Opacity>(shadow);
    return shadow;
}

/// A text of the screens of the cockpit only: empty, so drawn nowhere, until the HudSystem writes it.
ecs::entity_t createCockpitText(ecs::Ecs &ecs, engine::SceneId scene, engine::Surface surface,
                                render::FontId font, render::Color color)
{
    return createText(ecs, scene, engine::AnchorPoint::TopLeft, surface, font, 0.0f, color, 0.0f);
}

} // namespace

ecs::entity_t openHud(ecs::Ecs &ecs, engine::SceneId scene, const HudLook &look, std::uint8_t maxLives)
{
    Hud hud;
    hud.look = look;
    hud.topShadow = createShadow(ecs, scene, engine::AnchorPoint::Top, TopShadowHeight, TopShadow, Clear);
    hud.bottomShadow =
        createShadow(ecs, scene, engine::AnchorPoint::Bottom, BottomShadowHeight, Clear, BottomShadow);

    for (std::uint8_t index = 0; index < maxLives; ++index) {
        const ecs::entity_t life =
            createPart(ecs, scene, HudOrder, engine::AnchorPoint::BottomLeft, engine::Surface::HudShip);
        ecs.addComponent<engine::Sprite>(life, look.life, LifeSource, ScreenHud.life.x, ScreenHud.life.y);
        hud.lives.push_back(life);
    }
    constexpr engine::Surface ship = engine::Surface::HudShip;
    constexpr engine::Surface nav = engine::Surface::HudNav;
    constexpr engine::Surface aux = engine::Surface::HudAux;
    hud.pilot = createText(ecs, scene, engine::AnchorPoint::BottomLeft, ship, look.font, ScreenHud.pilot.size,
                           PilotColor, ScreenHud.pilot.spacing);
    hud.pilotVoice =
        createVoiceIcon(ecs, scene, engine::AnchorPoint::BottomLeft, ship, look, ScreenHud.pilotVoice);
    hud.score = createText(ecs, scene, engine::AnchorPoint::BottomLeft, ship, look.font, ScreenHud.score.size,
                           render::colors::White, ScreenHud.score.spacing);
    hud.health = createText(ecs, scene, engine::AnchorPoint::BottomLeft, ship, look.font,
                            ScreenHud.health.size, HealthColor, ScreenHud.health.spacing);
    hud.xp = createText(ecs, scene, engine::AnchorPoint::BottomLeft, ship, look.font, ScreenHud.xp.size,
                        XpColor, ScreenHud.xp.spacing);

    for (std::uint8_t index = 1; index < game::MaxPlayers; ++index) {
        const ecs::entity_t slot = createText(ecs, scene, engine::AnchorPoint::BottomRight, aux, look.font,
                                              ScreenHud.slot.size, AllySlotColor, ScreenHud.slot.spacing);
        const ecs::entity_t name = createText(ecs, scene, engine::AnchorPoint::BottomRight, aux, look.font,
                                              ScreenHud.ally.size, AllyNameColor, ScreenHud.ally.spacing);
        const ecs::entity_t score = createText(ecs, scene, engine::AnchorPoint::BottomRight, aux, look.font,
                                               ScreenHud.ally.size, AllyScoreColor, ScreenHud.ally.spacing);
        const ecs::entity_t voice =
            createVoiceIcon(ecs, scene, engine::AnchorPoint::BottomRight, aux, look, ScreenHud.allyVoice);
        hud.allies.push_back({slot, name, score, voice});
    }
    for (const float height : ScreenHud.bars) {
        const ecs::entity_t bar = createPart(ecs, scene, HudOrder, engine::AnchorPoint::BottomRight, nav);
        ecs.addComponent<engine::Box>(bar, ScreenHud.barWidth, height);
        ecs.addComponent<engine::Opacity>(bar, 0.0f);
        hud.signal.push_back(bar);
    }
    hud.ping = createText(ecs, scene, engine::AnchorPoint::BottomRight, nav, look.font, ScreenHud.ping.size,
                          PingColor, ScreenHud.ping.spacing);

    hud.waveLabel = createCockpitText(ecs, scene, nav, look.font, PilotColor);
    hud.wave = createCockpitText(ecs, scene, nav, look.font, render::colors::White);
    hud.boss = createCockpitText(ecs, scene, nav, look.font, BossColor);
    hud.modeLight = createPart(ecs, scene, HudOrder, engine::AnchorPoint::TopLeft, nav);
    ecs.addComponent<engine::Box>(hud.modeLight, 0.0f, 0.0f, OnlineLight);
    hud.mode = createCockpitText(ecs, scene, nav, look.font, PilotColor);
    hud.pilots = createCockpitText(ecs, scene, aux, look.font, PilotColor);

    constexpr engine::Surface holo = engine::Surface::Holo;
    hud.radarTitle = createCockpitText(ecs, scene, holo, look.font, RadarLight);
    hud.radarFrame = createPart(ecs, scene, HudOrder, engine::AnchorPoint::TopLeft, holo);
    ecs.addComponent<engine::Box>(hud.radarFrame, engine::Box{.color = RadarLight, .outline = RadarOutline});
    ecs.addComponent<engine::Opacity>(hud.radarFrame, 0.0f);
    for (std::size_t line = 0; line < RadarGridLines; ++line) {
        const ecs::entity_t grid = createPart(ecs, scene, ShadowOrder, engine::AnchorPoint::TopLeft, holo);
        ecs.addComponent<engine::Box>(grid, engine::Box{.color = RadarGrid});
        ecs.addComponent<engine::Opacity>(grid, 0.0f);
        hud.radarGrid.push_back(grid);
    }
    for (std::size_t index = 0; index < RadarBlips; ++index) {
        const ecs::entity_t blip = createPart(ecs, scene, HudOrder, engine::AnchorPoint::TopLeft, holo);
        ecs.addComponent<engine::Box>(blip);
        ecs.addComponent<engine::Opacity>(blip, 0.0f);
        hud.radarBlips.push_back(blip);
    }

    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<Hud>(entity, std::move(hud));
    return entity;
}

} // namespace rtype::client
