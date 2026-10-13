#include "rtype/engine/System/AnchorSystem.hpp"

namespace rtype::engine
{

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
    const render::Rectangle screen = _metrics.visibleArea();
    for (ecs::entity_t entity = 0; entity < _anchors.size(); ++entity) {
        if (!_anchors.has(entity)) {
            continue;
        }
        const render::Vector2 size = sizeOf(entity).value_or(render::Vector2{0.0f, 0.0f});
        const render::Vector2 position = computeAnchoredPosition(screen, _anchors.get(entity), size);
        _positions.emplace(entity, position.x, position.y);
    }
}

} // namespace rtype::engine
