#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Widget.hpp"

namespace rtype::engine
{

/// Writes the Text of the widgets that show a value (Toggle, Slider, Choice, TextField) from their value,
/// every frame: the text follows the setting even when it changes elsewhere.
class WidgetTextSystem : public ecs::System
{
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<Toggle> &_toggles;
    ecs::ComponentCache<Slider> &_sliders;
    ecs::ComponentCache<Choice> &_choices;
    ecs::ComponentCache<TextField> &_fields;

public:
    explicit WidgetTextSystem(ecs::ComponentCaches &caches);

    void update(float elapsed) override;
};

} // namespace rtype::engine
