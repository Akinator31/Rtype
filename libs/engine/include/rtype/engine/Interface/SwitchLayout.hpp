#pragma once

#include "rtype/engine/Interface/Style.hpp"
#include "rtype/render/Types.hpp"

#include <algorithm>

namespace rtype::engine
{

/// Where the parts of a switch are (see InterfaceBuilder::toggle()), computed in one place so that what is
/// drawn and what lights up under the pointer are the same thing.
struct SwitchLayout {
    /// Left edge of the "OFF" label, and of the "ON" label.
    float offX = 0.0f;
    float onX = 0.0f;
    /// The two ends of the link between the labels, where the dot slides.
    float linkLeft = 0.0f;
    float linkRight = 0.0f;
    /// Height of the labels.
    float labelHeight = 0.0f;
    /// What lights up the switch when the pointer is inside: the labels and the link, and only a small margin
    /// around them, as high as the labels and the dot.
    render::Rectangle hoverArea;

    /// Whether the point is over the switch.
    [[nodiscard]] bool contains(render::Vector2 point) const
    {
        return point.x >= hoverArea.x && point.x <= hoverArea.x + hoverArea.width && point.y >= hoverArea.y &&
               point.y <= hoverArea.y + hoverArea.height;
    }
};

/// Places the "OFF", link and "ON" group on the right of a row of the width of the style, centred on `centre`.
/// @param measure Gives the size of a text: measure(text, size, spacing), in game units.
template <typename Measure>
[[nodiscard]] SwitchLayout computeSwitchLayout(const SwitchStyle &style, render::Vector2 centre, Measure &&measure)
{
    const render::Vector2 off = measure("OFF", style.labelSize, style.labelSpacing);
    const render::Vector2 on = measure("ON", style.labelSize, style.labelSpacing);
    const float group = off.x + style.labelGap + style.linkWidth + style.labelGap + on.x;

    SwitchLayout layout;
    layout.offX = centre.x + (style.width / 2.0f) - group;
    layout.linkLeft = layout.offX + off.x + style.labelGap;
    layout.linkRight = layout.linkLeft + style.linkWidth;
    layout.onX = layout.linkRight + style.labelGap;
    layout.labelHeight = on.y;
    const float half = (std::max(on.y, 2.0f * style.dotRadius) / 2.0f) + style.hoverMarginY;
    layout.hoverArea = {layout.offX - style.hoverMargin, centre.y - half, group + (2.0f * style.hoverMargin),
                        2.0f * half};
    return layout;
}

} // namespace rtype::engine
