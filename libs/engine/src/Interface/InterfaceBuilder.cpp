#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Layout.hpp"
#include "rtype/engine/Component/Widget.hpp"

#include <utility>

namespace rtype::engine
{

namespace
{

// Drawing order inside the interface layer, over the background of a menu
constexpr int contentOrder = 1;

// ASCII: the font of the buttons (Orbitron) has no "←", which would be drawn as "?"
constexpr const char *backArrow = "<";

} // namespace

InterfaceBuilder::InterfaceBuilder(ecs::Ecs &ecs, SceneId scene, const InterfaceStyle &style)
    : _ecs(ecs), _style(style), _scene(scene)
{
}

ecs::entity_t InterfaceBuilder::element(Anchor anchor)
{
    const ecs::entity_t entity = _ecs.createEntity();
    _ecs.addComponent<SceneMember>(entity, _scene);
    _ecs.addComponent<Anchor>(entity, anchor);
    // Written by the AnchorSystem; added here so that the entity is drawn from its first frame
    _ecs.addComponent<ecs::Position>(entity);
    _ecs.addComponent<Drawable>(entity, Layer::Interface, contentOrder);
    return entity;
}

ecs::entity_t InterfaceBuilder::text(const std::string &text, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, text, _style.font, _style.textSize, _style.text);
    return entity;
}

ecs::entity_t InterfaceBuilder::boundText(std::function<std::string()> get, Anchor anchor)
{
    const ecs::entity_t entity = text(get ? get() : std::string(), anchor);
    _ecs.addComponent<BoundText>(entity, std::move(get));
    return entity;
}

ecs::entity_t InterfaceBuilder::title(const std::string &text, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, text, _style.title.font, _style.textSize, _style.text);
    _ecs.addComponent<Title>(entity);
    return entity;
}

ecs::entity_t InterfaceBuilder::boundTitle(std::function<std::string()> get, Anchor anchor)
{
    const ecs::entity_t entity = title(get ? get() : std::string(), anchor);
    _ecs.addComponent<BoundText>(entity, std::move(get));
    return entity;
}

ecs::entity_t InterfaceBuilder::slider(const std::string &caption, std::function<float()> get,
                                       std::function<void(float)> set, float step, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, caption, _style.slider.font, _style.slider.captionSize, _style.text);
    _ecs.addComponent<Slider>(entity, Slider{caption, std::move(get), std::move(set), step});
    _ecs.addComponent<SliderTrack>(entity);
    _ecs.addComponent<Focusable>(entity, _count++);
    return entity;
}

ecs::entity_t InterfaceBuilder::toggle(const std::string &caption, std::function<bool()> get,
                                       std::function<void(bool)> set, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, caption, _style.toggle.font, _style.toggle.captionSize, _style.text);
    _ecs.addComponent<Toggle>(entity, Toggle{caption, std::move(get), std::move(set)});
    _ecs.addComponent<SwitchTrack>(entity);
    _ecs.addComponent<Focusable>(entity, _count++);
    return entity;
}

ecs::entity_t InterfaceBuilder::gauge(const std::string &caption, std::function<float()> get, Anchor anchor)
{
    const GaugeStyle &style = _style.gauge;
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Gauge>(entity, Gauge{caption, std::move(get)});
    _ecs.addComponent<Extent>(entity, style.width, style.getHeight());
    return entity;
}

ecs::entity_t InterfaceBuilder::box(float width, float height, render::Color color, Anchor anchor,
                                    float outline)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Box>(entity, width, height, color, outline);
    return entity;
}

ecs::entity_t InterfaceBuilder::sprite(render::TextureId texture, render::Rectangle source, float width,
                                       float height, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Sprite>(entity, texture, source, width, height, render::colors::White);
    return entity;
}

ecs::entity_t InterfaceBuilder::button(const std::string &text, std::function<void()> onPress, Anchor anchor)
{
    const FlareStyle &style = _style.flare;
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, text, style.font, style.textSize, style.text);
    _ecs.addComponent<Panel>(entity, style.height);
    _ecs.addComponent<Focusable>(entity, _count++);
    _ecs.addComponent<Scale>(entity);
    _ecs.addComponent<Button>(entity, std::move(onPress));
    return entity;
}

ecs::entity_t InterfaceBuilder::backButton(std::function<void()> onPress, Anchor anchor)
{
    return button(backArrow, std::move(onPress), anchor);
}

ecs::entity_t InterfaceBuilder::header(const std::string &title, std::function<void()> onBack, Anchor anchor)
{
    const ecs::entity_t back = backButton(std::move(onBack), anchor);
    const float titleX = anchor.x + _style.flare.height + _style.headerSpacing;
    const float titleY = anchor.y + ((_style.flare.height - _style.textSize) / 2.0f);
    this->title(title, {anchor.point, titleX, titleY});
    return back;
}

} // namespace rtype::engine
