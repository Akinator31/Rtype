#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Components.hpp"
#include "rtype/input/TextInput.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace rtype::engine
{

/// Makes an entity a widget the player can focus, in a menu. The order sets the navigation with Up and Down.
struct Focusable {
    int order = 0;
};

/// The widget that has the focus in its scene. Set by the InterfaceSystem: at most one per scene.
struct Focused {};

/// Area of a widget the mouse can point at, centred on its position, in game units. Computed by the
/// HitboxSystem from its text.
struct Hitbox {
    /// Current size, including the Scale of the widget (a focused widget is bigger).
    float width = 0.0f;
    float height = 0.0f;
    /// Size at scale 1: layouts use it, so that widgets do not move when another one bounces.
    float baseWidth = 0.0f;
    float baseHeight = 0.0f;
};

/// A button: runs an action when it is confirmed or clicked.
struct Button {
    std::function<void()> onPress;
};

/// An on / off setting, shown as "Caption: On". Confirm and click switch it; Right turns it on, Left off.
///
/// The value is read and written through functions rather than copied, so that the widget always shows the
/// real setting, even when it is changed elsewhere (a shortcut key, for instance).
struct Toggle {
    std::string caption;
    std::function<bool()> get;
    std::function<void(bool)> set;
};

/// A value between 0 and 1, shown as a percentage: "Caption: 80%". Left, Right and the mouse wheel change it.
struct Slider {
    std::string caption;
    std::function<float()> get;
    std::function<void(float)> set;
    float step = 0.1f;
};

/// One option among several, shown as "Caption: Option". Left, Right, the mouse wheel and a click go through
/// the options, and around.
struct Choice {
    std::string caption;
    std::vector<std::string> options;
    /// Returns the index of the current option.
    std::function<std::size_t()> get;
    std::function<void(std::size_t)> set;
};

/// A text field, shown as "Caption: content". It is edited while it has the focus, with everything a
/// TextInput offers (cursor, selection, clipboard); Confirm submits it.
struct TextField {
    std::string caption;
    input::TextInput input;
    /// Called with the content when the player confirms the field. Can be empty.
    std::function<void(const std::string &)> onSubmit;
};

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

/// Label of a widget that shows a value: "Caption: value".
[[nodiscard]] std::string widgetLabel(const Toggle &toggle);
[[nodiscard]] std::string widgetLabel(const Slider &slider);
[[nodiscard]] std::string widgetLabel(const Choice &choice);
[[nodiscard]] std::string widgetLabel(const TextField &field);
/// Text before the content of a text field: "Caption: ".
[[nodiscard]] std::string fieldPrefix(const TextField &field);

} // namespace rtype::engine
