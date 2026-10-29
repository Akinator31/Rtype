#include "Game/Hud.hpp"

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

// The sizes are those of the Figma design, 1440 wide, scaled to the 1920 of the game: by 4/3

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
constexpr float LifeWidth = 40.0f;
constexpr float LifeHeight = 16.0f;

constexpr float PilotSize = 13.3f;
constexpr float PilotSpacing = 2.1f;
constexpr render::Color PilotColor{138, 144, 151, 255};

constexpr float ScoreSize = 34.7f;
constexpr float ScoreSpacing = 4.0f;

constexpr float AllySize = 12.0f;
constexpr float AllySpacing = 1.3f;
constexpr render::Color AllyNameColor{107, 113, 119, 255};
constexpr render::Color AllyScoreColor{201, 205, 210, 255};

constexpr float PingSize = 13.3f;
constexpr float PingSpacing = 1.3f;
constexpr render::Color PingColor{201, 205, 210, 255};

constexpr float BarWidth = 4.0f;
constexpr std::array<float, 4> BarHeights{6.7f, 10.7f, 14.7f, 18.7f};

/// An entity of the HUD, placed by the HudSystem from a point of the screen.
ecs::entity_t createPart(ecs::Ecs &ecs, engine::SceneId scene, int order, engine::AnchorPoint point)
{
    const ecs::entity_t part = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(part, scene);
    ecs.addComponent<engine::Anchor>(part, point);
    ecs.addComponent<ecs::Position>(part);
    ecs.addComponent<engine::Drawable>(part, engine::Layer::Interface, order);
    return part;
}

ecs::entity_t createText(ecs::Ecs &ecs, engine::SceneId scene, engine::AnchorPoint point, render::FontId font,
                         float size, render::Color color, float spacing)
{
    const ecs::entity_t text = createPart(ecs, scene, HudOrder, point);
    ecs.addComponent<engine::Text>(text, std::string(), font, size, color, spacing);
    return text;
}

ecs::entity_t createShadow(ecs::Ecs &ecs, engine::SceneId scene, engine::AnchorPoint point, float height,
                           render::Color top, render::Color bottom)
{
    const ecs::entity_t shadow = createPart(ecs, scene, ShadowOrder, point);
    ecs.addComponent<engine::Box>(shadow, engine::Box{.height = height, .color = top, .bottom = bottom});
    return shadow;
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
        const ecs::entity_t life = createPart(ecs, scene, HudOrder, engine::AnchorPoint::BottomLeft);
        ecs.addComponent<engine::Sprite>(life, look.life, LifeSource, LifeWidth, LifeHeight);
        hud.lives.push_back(life);
    }
    hud.pilot = createText(ecs, scene, engine::AnchorPoint::BottomLeft, look.font, PilotSize, PilotColor,
                           PilotSpacing);
    hud.score = createText(ecs, scene, engine::AnchorPoint::BottomLeft, look.font, ScoreSize,
                           render::colors::White, ScoreSpacing);

    for (std::uint8_t index = 1; index < game::MaxPlayers; ++index) {
        const ecs::entity_t name = createText(ecs, scene, engine::AnchorPoint::BottomRight, look.font,
                                              AllySize, AllyNameColor, 0.0f);
        const ecs::entity_t score = createText(ecs, scene, engine::AnchorPoint::BottomRight, look.font,
                                               AllySize, AllyScoreColor, AllySpacing);
        hud.allies.push_back({name, score});
    }
    for (const float height : BarHeights) {
        const ecs::entity_t bar = createPart(ecs, scene, HudOrder, engine::AnchorPoint::BottomRight);
        ecs.addComponent<engine::Box>(bar, BarWidth, height);
        ecs.addComponent<engine::Opacity>(bar, 0.0f);
        hud.signal.push_back(bar);
    }
    hud.ping =
        createText(ecs, scene, engine::AnchorPoint::BottomRight, look.font, PingSize, PingColor, PingSpacing);

    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<Hud>(entity, std::move(hud));
    return entity;
}

} // namespace rtype::client
