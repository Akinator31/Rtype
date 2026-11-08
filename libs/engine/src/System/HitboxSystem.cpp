#include "rtype/engine/System/HitboxSystem.hpp"

namespace rtype::engine
{

HitboxSystem::HitboxSystem(ecs::ComponentCaches &caches, const Metrics &metrics, const InterfaceStyle &style)
    : _metrics(metrics), _style(style), _focusables(use<Focusable>(caches)), _items(use<ColumnItem>(caches)),
      _texts(use<Text>(caches)), _panels(use<Panel>(caches)), _tracks(use<SliderTrack>(caches)),
      _switches(use<SwitchTrack>(caches)), _scales(use<Scale>(caches)), _hitboxes(use<Hitbox>(caches)),
      _fields(use<TextField>(caches)), _rows(use<ListRow>(caches)), _settingRows(use<SettingRow>(caches)),
      _keyRows(use<KeyRow>(caches))
{
}

void HitboxSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _texts.size(); ++entity) {
        // A disabled row of a list gets no focus, but is placed by its size as the others
        if (!_texts.has(entity) || (!_focusables.has(entity) && !_items.has(entity) && !_rows.has(entity))) {
            continue;
        }
        const render::Vector2 size = measure(entity);
        const float scale = _scales.has(entity) ? _scales.get(entity).value : 1.0f;
        _hitboxes.emplace(entity, size.x * scale, size.y * scale, size.x, size.y);
    }
}

render::Vector2 HitboxSystem::measure(ecs::entity_t entity) const
{
    const Text &text = _texts.get(entity);
    if (_rows.has(entity)) {
        return {_rows.get(entity).width, _style.list.height};
    }
    if (_settingRows.has(entity)) {
        return {_settingRows.get(entity).width, _style.setting.height};
    }
    if (_keyRows.has(entity)) {
        return {_keyRows.get(entity).width, _style.keys.rowHeight};
    }
    if (_fields.has(entity) && _fields.get(entity).type == TextFieldType::Line) {
        return {_style.field.width, _style.field.getHeight()};
    }
    if (_fields.has(entity) && _fields.get(entity).type == TextFieldType::Code) {
        return {_style.field.getCodeWidth(_fields.get(entity).input.maxLength()),
                _style.field.getCodeHeight()};
    }
    if (_panels.has(entity)) {
        const FlareStyle &style = _style.flare;
        const float width = _metrics.measureText(text.font, text.text, text.size, style.letterSpacing).x;
        return {style.computeWidth(width), style.height};
    }
    if (_tracks.has(entity)) {
        return {_style.slider.getAreaWidth(_tracks.get(entity).width), _style.slider.getHeight()};
    }
    if (_switches.has(entity)) {
        return {_style.toggle.width, _style.toggle.height};
    }

    const render::Vector2 size = _metrics.measureText(text.font, text.text, text.size, text.spacing);
    return {size.x + 2.0f * _style.padding, size.y + 2.0f * _style.padding};
}

} // namespace rtype::engine
