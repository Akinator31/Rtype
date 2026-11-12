#include "Hud.hpp"

#include "Visuals.hpp"

#include <string>

namespace orbit
{

namespace
{

namespace ecs = rtype::ecs;
namespace engine = rtype::engine;
using engine::AnchorPoint;
using rtype::render::Color;

constexpr float margin = 60.0f;
constexpr float labelSize = 24.0f;
constexpr Color labelColor{140, 170, 210, 255};
constexpr Color comboColor{255, 200, 90, 255};
constexpr float lostLifeOpacity = 0.15f;
constexpr float lifeSpacing = 70.0f;
constexpr int barOrder = 10;

/// The dark band behind the texts. Wider than the arena, for wide screens.
void spawnBar(ecs::Ecs &ecs, engine::SceneId scene)
{
    const ecs::entity_t bar =
        spawnDrawn(ecs, scene, ArenaWidth / 2.0f, FieldTop / 2.0f, engine::Layer::World, barOrder);
    ecs.addComponent<engine::Box>(bar, engine::Box{.width = 2.0f * ArenaWidth,
                                                   .height = FieldTop,
                                                   .color = {6, 8, 20, 240},
                                                   .bottom = Color{14, 20, 44, 220}});
}

/// The lives, as small paddles in the top right corner.
void spawnLives(ecs::Ecs &ecs, engine::SceneId scene)
{
    for (int index = 0; index < StartingLives; ++index) {
        const float x = ArenaWidth - margin - 30.0f - (static_cast<float>(index) * lifeSpacing);
        const ecs::entity_t icon =
            spawnDrawn(ecs, scene, x, FieldTop / 2.0f, engine::Layer::World, barOrder + 1);
        ecs.addComponent<engine::Box>(
            icon, engine::Box{.width = 52.0f, .height = 12.0f, .color = PaddleTop, .bottom = PaddleBottom});
        ecs.addComponent<engine::Opacity>(icon);
        ecs.addComponent<LifeIcon>(icon, index);
    }
}

/// "Press Space to launch", blinking, while the ball waits on the paddle.
void addLaunchHint(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, const Match &match)
{
    const ecs::entity_t hint = interface.boundText(
        [&match] { return match.ballWaiting ? std::string("PRESS SPACE TO LAUNCH") : std::string(); },
        {AnchorPoint::Center, 0.0f, 200.0f});
    engine::Text &look = ecs.getComponent<engine::Text>(hint);
    look.size = 30.0f;
    look.spacing = 6.0f;
    ecs.addComponent<engine::Tween>(hint, pingPong(engine::TweenTarget::Opacity, 1.0f, 0.25f, 0.7f));
}

} // namespace

void buildHud(engine::InterfaceBuilder &interface, ecs::Ecs &ecs, engine::SceneId scene, const Match &match)
{
    spawnBar(ecs, scene);
    spawnLives(ecs, scene);
    styledText(interface, ecs, "SCORE", {AnchorPoint::TopLeft, margin, 14.0f}, labelSize, labelColor);
    interface.boundText([&match] { return std::to_string(match.score); },
                        {AnchorPoint::TopLeft, margin, 40.0f});
    const ecs::entity_t combo = interface.boundText(
        [&match] { return match.combo >= 2 ? "COMBO x" + std::to_string(match.combo) : std::string(); },
        {AnchorPoint::TopLeft, margin + 300.0f, 46.0f});
    ecs.getComponent<engine::Text>(combo).color = comboColor;
    styledText(interface, ecs, "LEVEL", {AnchorPoint::Top, 0.0f, 14.0f}, labelSize, labelColor);
    interface.boundText([&match] { return std::to_string(match.level); }, {AnchorPoint::Top, 0.0f, 40.0f});
    styledText(interface, ecs, "LIVES", {AnchorPoint::TopRight, -margin, 14.0f}, labelSize, labelColor);
    addLaunchHint(interface, ecs, match);
}

LifeIconSystem::LifeIconSystem(ecs::ComponentCaches &caches, const Match &match)
    : _match(match), _icons(use<LifeIcon>(caches)), _opacities(use<engine::Opacity>(caches))
{
}

void LifeIconSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t icon = 0; icon < _icons.size(); ++icon) {
        if (_icons.has(icon) && _opacities.has(icon)) {
            _opacities.get(icon).value = _icons.get(icon).index < _match.lives ? 1.0f : lostLifeOpacity;
        }
    }
}

} // namespace orbit
