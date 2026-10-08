#include "rtype/engine/Widgets.hpp"

#include <cmath>

namespace rtype::engine
{

std::string widgetLabel(const Toggle &toggle)
{
    return toggle.caption + ": " + (toggle.get && toggle.get() ? "On" : "Off");
}

std::string widgetLabel(const Slider &slider)
{
    const float value = slider.get ? slider.get() : 0.0f;
    return slider.caption + ": " + std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
}

std::string widgetLabel(const Choice &choice)
{
    const std::size_t index = choice.get ? choice.get() : 0;
    return choice.caption + ": " + (index < choice.options.size() ? choice.options[index] : std::string("?"));
}

std::string fieldPrefix(const TextField &field)
{
    return field.caption + ": ";
}

std::string widgetLabel(const TextField &field)
{
    return fieldPrefix(field) + field.input.text();
}

WidgetTextSystem::WidgetTextSystem(ecs::ComponentCaches &caches)
    : _texts(use<Text>(caches)), _toggles(use<Toggle>(caches)), _sliders(use<Slider>(caches)),
      _choices(use<Choice>(caches)), _fields(use<TextField>(caches))
{
}

void WidgetTextSystem::update(float /*elapsed*/)
{
    for (ecs::entity_t entity = 0; entity < _texts.size(); ++entity) {
        if (!_texts.has(entity)) {
            continue;
        }
        std::string &text = _texts.get(entity).text;
        if (_toggles.has(entity)) {
            text = widgetLabel(_toggles.get(entity));
        } else if (_sliders.has(entity)) {
            text = widgetLabel(_sliders.get(entity));
        } else if (_choices.has(entity)) {
            text = widgetLabel(_choices.get(entity));
        } else if (_fields.has(entity)) {
            text = widgetLabel(_fields.get(entity));
        }
    }
}

} // namespace rtype::engine
