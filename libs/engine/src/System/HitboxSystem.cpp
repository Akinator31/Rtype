#include "rtype/engine/System/HitboxSystem.hpp"

namespace rtype::engine
{

HitboxSystem::HitboxSystem(ecs::ComponentCaches &caches, const Metrics &metrics, const InterfaceStyle &style)
    : _metrics(metrics), _style(style), _focusables(use<Focusable>(caches)), _items(use<ColumnItem>(caches)),
      _texts(use<Text>(caches)), _scales(use<Scale>(caches)), _hitboxes(use<Hitbox>(caches))
{
}

void HitboxSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _texts.size(); ++entity) {
        if (!_texts.has(entity) || (!_focusables.has(entity) && !_items.has(entity))) {
            continue;
        }
        const Text &text = _texts.get(entity);
        const render::Vector2 size = _metrics.measureText(text.font, text.text, text.size);
        const float scale = _scales.has(entity) ? _scales.get(entity).value : 1.0f;
        const float width = size.x + 2.0f * _style.padding;
        const float height = size.y + 2.0f * _style.padding;
        _hitboxes.emplace(entity, width * scale, height * scale, width, height);
    }
}

} // namespace rtype::engine
