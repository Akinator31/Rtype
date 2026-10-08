#include "rtype/engine/Component/Widget.hpp"

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

} // namespace rtype::engine
