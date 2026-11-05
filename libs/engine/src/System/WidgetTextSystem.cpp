#include "rtype/engine/System/WidgetTextSystem.hpp"

namespace rtype::engine
{

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
        } else if (_fields.has(entity) && !isFormField(_fields.get(entity))) {
            // A field of a form is left alone: its text stays its caption, and it draws its content itself
            text = widgetLabel(_fields.get(entity));
        }
    }
}

} // namespace rtype::engine
