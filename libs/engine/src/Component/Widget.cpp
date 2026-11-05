#include "rtype/engine/Component/Widget.hpp"

#include <algorithm>
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
    if (field.caption.empty()) {
        return {};
    }
    return field.caption + (field.type == TextFieldType::Console ? " " : ": ");
}

std::string fieldContent(const TextField &field, std::size_t bytes)
{
    const std::string &text = field.input.text();
    const std::string shown = text.substr(0, std::min(bytes, text.size()));
    if (!field.input.isSecret()) {
        return shown;
    }
    // One mask per character: the bytes that do not continue a character of UTF-8
    const auto characters = std::ranges::count_if(
        shown, [](char byte) { return (static_cast<unsigned char>(byte) & 0xC0U) != 0x80U; });
    return std::string(static_cast<std::size_t>(characters), '*');
}

bool isFormField(const TextField &field)
{
    return field.type == TextFieldType::Line || field.type == TextFieldType::Code;
}

std::string widgetLabel(const TextField &field)
{
    return fieldPrefix(field) + fieldContent(field);
}

} // namespace rtype::engine
