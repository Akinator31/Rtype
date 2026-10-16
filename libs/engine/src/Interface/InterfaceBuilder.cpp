#include "rtype/engine/Interface/InterfaceBuilder.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"

#include <utility>

namespace rtype::engine
{

namespace
{

// Drawing order inside the interface layer, over the background of a menu
constexpr int contentOrder = 1;

// "←" in UTF-8
constexpr const char *leftwardsArrow = "\xE2\x86\x90";

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

ecs::entity_t InterfaceBuilder::createButton(const std::string &text, float width,
                                             std::function<void()> onPress, Anchor anchor)
{
    const ecs::entity_t entity = element(anchor);
    _ecs.addComponent<Text>(entity, text, _style.button.font, _style.button.textSize, _style.text);
    _ecs.addComponent<Panel>(entity, width, _style.button.height);
    _ecs.addComponent<Focusable>(entity, _count++);
    _ecs.addComponent<Scale>(entity);
    _ecs.addComponent<Button>(entity, std::move(onPress));
    return entity;
}

ecs::entity_t InterfaceBuilder::button(const std::string &text, std::function<void()> onPress, Anchor anchor)
{
    return createButton(text, _style.button.width, std::move(onPress), anchor);
}

ecs::entity_t InterfaceBuilder::backButton(std::function<void()> onPress, Anchor anchor)
{
    return createButton(leftwardsArrow, _style.button.height, std::move(onPress), anchor);
}

} // namespace rtype::engine
