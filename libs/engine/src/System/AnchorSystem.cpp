#include "rtype/engine/System/AnchorSystem.hpp"

namespace rtype::engine
{

namespace
{

// Where the point is, from 0 (left or top) to 1 (right or bottom)
render::Vector2 fractionOf(AnchorPoint point)
{
    switch (point) {
    case AnchorPoint::TopLeft:
        return {0.0f, 0.0f};
    case AnchorPoint::Top:
        return {0.5f, 0.0f};
    case AnchorPoint::TopRight:
        return {1.0f, 0.0f};
    case AnchorPoint::Left:
        return {0.0f, 0.5f};
    case AnchorPoint::Center:
        return {0.5f, 0.5f};
    case AnchorPoint::Right:
        return {1.0f, 0.5f};
    case AnchorPoint::BottomLeft:
        return {0.0f, 1.0f};
    case AnchorPoint::Bottom:
        return {0.5f, 1.0f};
    case AnchorPoint::BottomRight:
        return {1.0f, 1.0f};
    }
    return {0.5f, 0.5f};
}

} // namespace

AnchorSystem::AnchorSystem(ecs::ComponentCaches &caches, const Metrics &metrics)
    : _metrics(metrics), _anchors(use<Anchor>(caches)), _positions(use<ecs::Position>(caches)),
      _hitboxes(use<Hitbox>(caches)), _texts(use<Text>(caches)), _sprites(use<Sprite>(caches)),
      _boxes(use<Box>(caches))
{
}

std::optional<render::Vector2> AnchorSystem::sizeOf(ecs::entity_t entity) const
{
    if (_hitboxes.has(entity)) {
        const Hitbox &hitbox = _hitboxes.get(entity);
        return render::Vector2{hitbox.baseWidth, hitbox.baseHeight};
    }
    if (_texts.has(entity)) {
        const Text &text = _texts.get(entity);
        return _metrics.measureText(text.font, text.text, text.size);
    }
    if (_sprites.has(entity)) {
        return render::Vector2{_sprites.get(entity).width, _sprites.get(entity).height};
    }
    if (_boxes.has(entity)) {
        return render::Vector2{_boxes.get(entity).width, _boxes.get(entity).height};
    }
    return std::nullopt;
}

void AnchorSystem::update(float /*elapsed*/)
{
    const render::Vector2 game = _metrics.gameSize();
    for (ecs::entity_t entity = 0; entity < _anchors.size(); ++entity) {
        if (!_anchors.has(entity)) {
            continue;
        }
        const Anchor &anchor = _anchors.get(entity);
        const render::Vector2 fraction = fractionOf(anchor.point);
        const render::Vector2 size = sizeOf(entity).value_or(render::Vector2{0.0f, 0.0f});
        // Entities are drawn centred on their position: move the centre away from the anchored point
        _positions.emplace(entity, fraction.x * game.x + anchor.x + (0.5f - fraction.x) * size.x,
                           fraction.y * game.y + anchor.y + (0.5f - fraction.y) * size.y);
    }
}

} // namespace rtype::engine
