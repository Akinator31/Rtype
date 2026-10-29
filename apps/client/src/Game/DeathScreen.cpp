#include "Game/DeathScreen.hpp"

#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"

#include <algorithm>
#include <utility>

namespace rtype::client
{

namespace
{

/// Over the HUD of the game, which the veil darkens, and under the toasts.
constexpr int VeilOrder = 10;
constexpr int ContentOrder = 11;

constexpr render::Color VeilColor{0, 0, 0, 128};

constexpr render::Color CaptionColor{201, 205, 210, 255};
constexpr float CaptionSize = 18.0f;
constexpr float CaptionSpacing = 4.3f;

/// The pictures of the lives, the whole texture, and their size on the screen. The space between the caption
/// and the pictures is the space between two pictures.
constexpr render::Rectangle LifeSource{0.0f, 0.0f, 120.0f, 48.0f};
constexpr float LifeWidth = 54.0f;
constexpr float LifeHeight = 22.0f;
constexpr float LivesGap = 18.0f;

/// Distances from the middle of the band of the banner to the row of the lives, where the detail of a banner
/// is, and from the bottom of the band to the caption of the countdown and to the centre of its ring.
constexpr float LivesBelowBandCentre = 45.0f;
constexpr float RespawnCaptionBelowBand = 90.0f;
constexpr float RingBelowBand = 187.0f;

constexpr float RingRadius = 72.0f;
constexpr float RingThickness = 3.5f;
constexpr float CountdownSize = 50.0f;

/// An entity of the death screen, centred at an offset from the centre of the screen, invisible until the
/// DeathScreenSystem fades it in.
ecs::entity_t createPart(ecs::Ecs &ecs, engine::SceneId scene, int order, render::Vector2 offset)
{
    const ecs::entity_t part = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(part, scene);
    ecs.addComponent<engine::Anchor>(part, engine::AnchorPoint::Center, offset.x, offset.y);
    ecs.addComponent<ecs::Position>(part);
    ecs.addComponent<engine::Drawable>(part, engine::Layer::Interface, order);
    ecs.addComponent<engine::Opacity>(part, 0.0f);
    return part;
}

ecs::entity_t createLife(ecs::Ecs &ecs, engine::SceneId scene, render::TextureId texture, int order,
                         render::Vector2 offset)
{
    const ecs::entity_t life = createPart(ecs, scene, order, offset);
    ecs.addComponent<engine::Sprite>(life, texture, LifeSource, LifeWidth, LifeHeight);
    return life;
}

/// The caption "LIVES LEFT", then one picture per life of the start, lit for the lives left, centred as a
/// whole. The life just lost is a life lost under a life left, which the DeathScreenSystem puts out.
void createLives(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics,
                 const DeathScreenLook &look, const Death &death, float y, DeathScreen &screen)
{
    const std::uint8_t maxLives = std::max(death.maxLives, static_cast<std::uint8_t>(death.lives + 1));
    const float captionWidth =
        metrics.measureText(look.captionFont, look.livesCaption, CaptionSize, CaptionSpacing).x;
    const float width = captionWidth + (static_cast<float>(maxLives) * (LivesGap + LifeWidth));
    const float left = -width / 2.0f;

    const ecs::entity_t caption = createPart(ecs, scene, ContentOrder, {left + (captionWidth / 2.0f), y});
    ecs.addComponent<engine::Text>(caption, look.livesCaption, look.captionFont, CaptionSize, CaptionColor,
                                   CaptionSpacing);
    screen.content.push_back(caption);

    for (std::uint8_t index = 0; index < maxLives; ++index) {
        const float x = left + captionWidth + LivesGap + (static_cast<float>(index) * (LifeWidth + LivesGap)) +
                        (LifeWidth / 2.0f);
        const bool isLeft = index < death.lives;
        screen.content.push_back(
            createLife(ecs, scene, isLeft ? look.life : look.lostLife, ContentOrder, {x, y}));
        if (index == death.lives) {
            screen.lostLife = createLife(ecs, scene, look.life, ContentOrder + 1, {x, y});
        }
    }
}

/// The caption "RESPAWN", and under it the seconds left in a ring.
void createCountdown(ecs::Ecs &ecs, engine::SceneId scene, const DeathScreenLook &look, float bandBottom,
                     DeathScreen &screen)
{
    const ecs::entity_t caption =
        createPart(ecs, scene, ContentOrder, {0.0f, bandBottom + RespawnCaptionBelowBand});
    ecs.addComponent<engine::Text>(caption, look.respawnCaption, look.captionFont, CaptionSize, CaptionColor,
                                   CaptionSpacing);
    screen.content.push_back(caption);

    const render::Vector2 ringCentre{0.0f, bandBottom + RingBelowBand};
    screen.ring = createPart(ecs, scene, ContentOrder, ringCentre);
    ecs.addComponent<engine::Ring>(screen.ring, RingRadius, RingThickness);
    ecs.addComponent<engine::Extent>(screen.ring, 2.0f * RingRadius, 2.0f * RingRadius);
    screen.content.push_back(screen.ring);

    // Written by the DeathScreenSystem
    screen.countdown = createPart(ecs, scene, ContentOrder, ringCentre);
    ecs.addComponent<engine::Text>(screen.countdown, std::string(), look.countdownFont, CountdownSize);
    ecs.addComponent<engine::Scale>(screen.countdown, 1.0f);
    screen.content.push_back(screen.countdown);
}

} // namespace

ecs::entity_t openDeathScreen(ecs::Ecs &ecs, engine::SceneId scene, const engine::Metrics &metrics,
                              const engine::ToastStyle &banner, const DeathScreenLook &look,
                              const Death &death)
{
    DeathScreen screen;
    screen.death = death;
    const render::Rectangle area = metrics.visibleArea();

    screen.veil = createPart(ecs, scene, VeilOrder, {0.0f, 0.0f});
    ecs.addComponent<engine::Box>(screen.veil, area.width, area.height, VeilColor);

    // The banner is placed from the top of the screen, the parts from its centre
    const float bandCentre = (banner.bannerCentre - 0.5f) * area.height;
    createLives(ecs, scene, metrics, look, death, bandCentre + LivesBelowBandCentre, screen);
    createCountdown(ecs, scene, look, bandCentre + (banner.bannerHeight / 2.0f), screen);

    const ecs::entity_t entity = ecs.createEntity();
    ecs.addComponent<engine::SceneMember>(entity, scene);
    ecs.addComponent<DeathScreen>(entity, std::move(screen));
    return entity;
}

} // namespace rtype::client
