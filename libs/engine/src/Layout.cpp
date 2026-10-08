#include "rtype/engine/Layout.hpp"

#include "rtype/ecs/Ecs.hpp"

#include <algorithm>
#include <vector>

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

ColumnLayoutSystem::ColumnLayoutSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs)
    : _ecs(ecs), _columns(use<Column>(caches)), _items(use<ColumnItem>(caches)),
      _parents(use<ecs::Parent>(caches)), _hitboxes(use<Hitbox>(caches)),
      _locals(use<ecs::LocalPosition>(caches))
{
}

void ColumnLayoutSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t column = 0; column < _columns.size(); ++column) {
        if (!_columns.has(column)) {
            continue;
        }
        const ecs::EntityRef self = _ecs.ref(column);
        std::vector<ecs::entity_t> children;
        for (ecs::entity_t item = 0; item < _items.size(); ++item) {
            if (_items.has(item) && _parents.has(item) && _parents.get(item).entity == self &&
                _hitboxes.has(item) && _locals.has(item)) {
                children.push_back(item);
            }
        }
        std::ranges::sort(children, {}, [this](ecs::entity_t item) { return _items.get(item).index; });

        // Heights without the focus scale: the column must not move when a widget bounces
        const float spacing = _columns.get(column).spacing;
        float total = 0.0f;
        for (const ecs::entity_t item : children) {
            total += _hitboxes.get(item).baseHeight;
        }
        total += spacing * static_cast<float>(children.empty() ? 0 : children.size() - 1);

        float top = -total / 2.0f;
        for (const ecs::entity_t item : children) {
            const float height = _hitboxes.get(item).baseHeight;
            _locals.get(item) = {0.0f, top + height / 2.0f};
            top += height + spacing;
        }
    }
}

} // namespace rtype::engine
