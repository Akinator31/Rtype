#include "rtype/engine/System/InterfaceSystem.hpp"

#include "rtype/ecs/Ecs.hpp"
#include "rtype/engine/Interface/SwitchLayout.hpp"

#include <algorithm>
#include <cmath>

namespace rtype::engine
{

namespace
{

constexpr float focusDuration = 0.25f;
constexpr float unfocusDuration = 0.12f;

} // namespace

InterfaceSystem::InterfaceSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, Scenes &scenes,
                                 input::InputSource &input, const Metrics &metrics,
                                 const InterfaceStyle &style, input::ActionMap<Navigation> navigation)
    : _ecs(ecs), _scenes(scenes), _input(input), _metrics(metrics), _style(style),
      _navigation(std::move(navigation)), _focusables(use<Focusable>(caches)), _focused(use<Focused>(caches)),
      _hitboxes(use<Hitbox>(caches)), _positions(use<ecs::Position>(caches)), _scales(use<Scale>(caches)),
      _tweens(use<Tween>(caches)), _panels(use<Panel>(caches)), _buttons(use<Button>(caches)),
      _toggles(use<Toggle>(caches)), _sliders(use<Slider>(caches)), _tracks(use<SliderTrack>(caches)), _switches(use<SwitchTrack>(caches)), _texts(use<Text>(caches)),
      _choices(use<Choice>(caches)), _fields(use<TextField>(caches)), _sounds(use<SoundEffect>(caches))
{
}

// The visible widgets of the scene that receives the input, in navigation order
std::vector<ecs::entity_t> InterfaceSystem::widgets()
{
    std::vector<ecs::entity_t> result;
    for (ecs::entity_t entity = 0; entity < _focusables.size(); ++entity) {
        if (_focusables.has(entity) && _scenes.receivesInput(entity) && !ecs::isHidden(_ecs, entity)) {
            result.push_back(entity);
        }
    }
    std::ranges::stable_sort(result, {},
                             [this](ecs::entity_t entity) { return _focusables.get(entity).order; });
    return result;
}

bool InterfaceSystem::contains(ecs::entity_t widget, render::Vector2 point) const
{
    if (!_hitboxes.has(widget) || !_positions.has(widget)) {
        return false;
    }
    const Hitbox &box = _hitboxes.get(widget);
    const ecs::Position &centre = _positions.get(widget);
    return std::abs(point.x - centre.x) <= box.width / 2.0f &&
           std::abs(point.y - centre.y) <= box.height / 2.0f;
}

void InterfaceSystem::setFocus(const std::vector<ecs::entity_t> &widgets, ecs::entity_t widget)
{
    for (const ecs::entity_t other : widgets) {
        if (other != widget && _focused.has(other)) {
            _focused.erase(other);
            animateUnfocus(other);
        }
    }
    if (_focused.has(widget)) {
        return;
    }
    _focused.emplace(widget);
    animateFocus(widget);
}

/// Widgets with a Panel light up instead (see lightPanels()); a slider grows its handle; a console field stays
/// still, as a command line.
bool InterfaceSystem::keepsSize(ecs::entity_t widget) const
{
    return _panels.has(widget) || _tracks.has(widget) || _switches.has(widget) ||
           (_fields.has(widget) && _fields.get(widget).type == TextFieldType::Console);
}

void InterfaceSystem::animateFocus(ecs::entity_t widget)
{
    if (keepsSize(widget)) {
        return;
    }
    // A small bounce: grows slightly past the focus scale, then settles on it
    const float from = _scales.has(widget) ? _scales.get(widget).value : 1.0f;
    _tweens.emplace(widget, Tween{.target = TweenTarget::Scale,
                                  .from = from,
                                  .to = _style.focusScale,
                                  .duration = focusDuration,
                                  .easing = Easing::EaseOutBack});
}

void InterfaceSystem::animateUnfocus(ecs::entity_t widget)
{
    if (keepsSize(widget)) {
        return;
    }
    const float from = _scales.has(widget) ? _scales.get(widget).value : 1.0f;
    _tweens.emplace(widget, Tween{.target = TweenTarget::Scale,
                                  .from = from,
                                  .to = 1.0f,
                                  .duration = unfocusDuration,
                                  .easing = Easing::EaseOut});
}

bool InterfaceSystem::isLit(ecs::entity_t widget, std::optional<ecs::entity_t> hovered) const
{
    return _pointerMode ? hovered == widget : _focused.has(widget);
}

void InterfaceSystem::lightPanels(const std::vector<ecs::entity_t> &widgets,
                                  std::optional<ecs::entity_t> hovered, bool appeared)
{
    for (const ecs::entity_t widget : widgets) {
        if (!_panels.has(widget)) {
            continue;
        }
        const float target = isLit(widget, hovered) ? 1.0f : 0.0f;
        const bool fading = _tweens.has(widget) && _tweens.get(widget).target == TweenTarget::Highlight;
        const float heading = fading ? _tweens.get(widget).to : _panels.get(widget).highlight;
        if (heading != target) {
            _tweens.emplace(widget, Tween{.target = TweenTarget::Highlight,
                                          .from = _panels.get(widget).highlight,
                                          .to = target,
                                          .duration = computeFadeDuration(_panels.get(widget), target),
                                          .easing = Easing::EaseOut});
            if (target == 1.0f && !appeared) {
                playSound(widget, _style.button.hoverSound);
            }
        }
    }
}

/// A Flare takes its time to trace its line when it lights up, and goes out quickly.
float InterfaceSystem::computeFadeDuration(const Panel &panel, float target) const
{
    if (panel.type != PanelType::Flare) {
        return _style.button.fadeDuration;
    }
    return target > 0.0f ? _style.flare.traceDuration : _style.flare.fadeDuration;
}

bool InterfaceSystem::isFlare(ecs::entity_t widget) const
{
    return _panels.has(widget) && _panels.get(widget).type == PanelType::Flare;
}

bool InterfaceSystem::isPressDelayed(ecs::entity_t widget) const
{
    return std::ranges::any_of(_delayedPresses,
                               [widget](const DelayedPress &press) { return press.widget == widget; });
}

/// The actions whose delay is over run after the update, if their button still exists.
void InterfaceSystem::runDelayedPresses(float elapsed)
{
    for (DelayedPress &press : _delayedPresses) {
        press.delay -= elapsed;
        if (press.delay <= 0.0f && _ecs.isAlive(press.widget) && press.action) {
            _ecs.defer(std::move(press.action));
        }
    }
    std::erase_if(_delayedPresses, [](const DelayedPress &press) { return press.delay <= 0.0f; });
}

/// The highlight of a pressed Flare jumps above 1, then comes back to its lit look: its star bursts.
void InterfaceSystem::burstPanel(ecs::entity_t widget)
{
    _tweens.emplace(widget, Tween{.target = TweenTarget::Highlight,
                                  .from = _style.flare.pressedHighlight,
                                  .to = 1.0f,
                                  .duration = _style.flare.pressDuration,
                                  .easing = Easing::EaseOut});
}

void InterfaceSystem::playSound(ecs::entity_t widget, const std::optional<SoundEffect> &sound)
{
    if (sound) {
        _sounds.emplace(widget, *sound);
    }
}

void InterfaceSystem::updatePointerMode(bool mouseMoved)
{
    if (mouseMoved) {
        _pointerMode = true;
    }
    for (const Navigation action :
         {Navigation::Up, Navigation::Down, Navigation::Left, Navigation::Right, Navigation::Confirm}) {
        if (_navigation.wasPressed(action)) {
            _pointerMode = false;
        }
    }
}

// The actions are copied into the deferred calls: the components may move in memory before the calls run
void InterfaceSystem::activate(ecs::entity_t widget)
{
    if (_buttons.has(widget) && isFlare(widget)) {
        if (isPressDelayed(widget)) {
            return;
        }
        playSound(widget, _style.button.clickSound);
        burstPanel(widget);
        _delayedPresses.push_back({widget, _style.flare.pressDelay, _buttons.get(widget).onPress});
    } else if (_buttons.has(widget)) {
        playSound(widget, _style.button.clickSound);
        if (auto onPress = _buttons.get(widget).onPress) {
            _ecs.defer(std::move(onPress));
        }
    } else if (_toggles.has(widget)) {
        const Toggle toggle = _toggles.get(widget);
        _ecs.defer([toggle] { toggle.set(!toggle.get()); });
    } else if (_choices.has(widget)) {
        adjust(widget, 1);
    } else if (_fields.has(widget)) {
        const TextField &field = _fields.get(widget);
        if (auto onSubmit = field.onSubmit) {
            _ecs.defer([onSubmit, text = field.input.text()] { onSubmit(text); });
        }
    }
}

void InterfaceSystem::click(ecs::entity_t widget)
{
    // A click on a text field only gives it the focus: it does not submit it
    if (!_fields.has(widget)) {
        activate(widget);
    }
}

void InterfaceSystem::adjust(ecs::entity_t widget, int direction)
{
    if (_toggles.has(widget)) {
        const Toggle toggle = _toggles.get(widget);
        _ecs.defer([toggle, direction] { toggle.set(direction > 0); });
    } else if (_sliders.has(widget)) {
        const Slider slider = _sliders.get(widget);
        _ecs.defer([slider, direction] {
            // Rounded to the step, so that repeated additions do not drift (0.1 + 0.2 is not 0.3 in float)
            const float step = std::max(slider.step, 0.001f);
            const float value = slider.get() + static_cast<float>(direction) * step;
            slider.set(std::clamp(std::round(value / step) * step, 0.0f, 1.0f));
        });
    } else if (_choices.has(widget)) {
        const Choice choice = _choices.get(widget);
        if (choice.options.empty()) {
            return;
        }
        _ecs.defer([choice, direction] {
            const std::size_t count = choice.options.size();
            const std::size_t current = std::min(choice.get(), count - 1);
            choice.set(direction > 0 ? (current + 1) % count : (current + count - 1) % count);
        });
    }
}

void InterfaceSystem::updateSliders(const std::vector<ecs::entity_t> &widgets,
                                    std::optional<ecs::entity_t> hovered, render::Vector2 pointer,
                                    float elapsed)
{
    const bool isMouseHeld = _input.isMouseButtonDown(input::MouseButton::Left);
    if (!isMouseHeld) {
        _dragged.reset();
    } else if (hovered && _navigation.wasPressed(Navigation::Click) && _sliders.has(*hovered) &&
               _tracks.has(*hovered)) {
        _dragged = *hovered;
    }
    if (_dragged && (!_ecs.isAlive(*_dragged) || !_sliders.has(*_dragged) || !_positions.has(*_dragged))) {
        _dragged.reset();
    }
    if (_dragged) {
        // The handle follows the pointer along the rail, to the hundredth
        const float width = _style.slider.width;
        const float left = _positions.get(*_dragged).x - (width / 2.0f);
        const float value = std::round(std::clamp((pointer.x - left) / width, 0.0f, 1.0f) * 100.0f) / 100.0f;
        const Slider slider = _sliders.get(*_dragged);
        if (slider.get && slider.set && std::abs(slider.get() - value) > 0.001f) {
            _ecs.defer([slider, value] { slider.set(value); });
        }
    }

    const float step = elapsed / std::max(_style.slider.fadeDuration, 0.001f);
    const auto approach = [](float current, float target, float amount) {
        return current + std::clamp(target - current, -amount, amount);
    };
    for (const ecs::entity_t widget : widgets) {
        if (_tracks.has(widget)) {
            // The handle lights up only while the pointer is over it, the caption also with the focus
            SliderTrack &track = _tracks.get(widget);
            const bool isGrabbed = _dragged == widget;
            const bool isOver = _pointerMode && isOverHandle(widget, pointer);
            track.hover = approach(track.hover, isGrabbed || isOver ? 1.0f : 0.0f, step);
            track.grab = approach(track.grab, isGrabbed ? 1.0f : 0.0f, step);
            track.focus = approach(track.focus, isLit(widget, hovered) ? 1.0f : 0.0f, step);
        }
        if (_switches.has(widget) && _toggles.has(widget)) {
            SwitchTrack &track = _switches.get(widget);
            const Toggle &toggle = _toggles.get(widget);
            const float slide = elapsed / std::max(_style.toggle.slideDuration, 0.001f);
            track.hover = approach(track.hover, _pointerMode && isOverSwitch(widget, pointer) ? 1.0f : 0.0f, step);
            track.focus = approach(track.focus, isLit(widget, hovered) ? 1.0f : 0.0f, step);
            track.position = approach(track.position, toggle.get && toggle.get() ? 1.0f : 0.0f, slide);
        }
    }
}

bool InterfaceSystem::isOverHandle(ecs::entity_t widget, render::Vector2 pointer) const
{
    if (!_sliders.has(widget) || !_positions.has(widget)) {
        return false;
    }
    const SliderStyle &style = _style.slider;
    const Slider &slider = _sliders.get(widget);
    const ecs::Position &centre = _positions.get(widget);
    const float value = std::clamp(slider.get ? slider.get() : 0.0f, 0.0f, 1.0f);
    const float x = centre.x - (style.width / 2.0f) + (value * style.width);
    const float y = style.getRailY(centre.y);
    const float reach = style.handleHoverRadius + style.handleRingGap;
    return std::hypot(pointer.x - x, pointer.y - y) <= reach;
}

bool InterfaceSystem::isOverSwitch(ecs::entity_t widget, render::Vector2 pointer) const
{
    if (!_texts.has(widget) || !_positions.has(widget)) {
        return false;
    }
    // The group on the right of the row: "OFF", the link, "ON", and only a small margin around them
    const render::FontId font = _texts.get(widget).font;
    const SwitchLayout layout = computeSwitchLayout(
        _style.toggle, {_positions.get(widget).x, _positions.get(widget).y},
        [&](const char *label, float size, float spacing) {
            return _metrics.measureText(font, label, size, spacing);
        });
    return layout.contains(pointer);
}

void InterfaceSystem::update(float elapsed)
{
    _navigation.update(_input, elapsed);
    runDelayedPresses(elapsed);
    const input::MousePosition mouse = _input.mousePosition();
    const bool mouseMoved = mouse.x != _previousMouse.x || mouse.y != _previousMouse.y;
    _previousMouse = {mouse.x, mouse.y};

    const std::vector<ecs::entity_t> list = widgets();
    if (list.empty()) {
        // A scene without widgets, such as the game, still goes back: its pause menu, for instance
        if (_navigation.wasPressed(Navigation::Back)) {
            _ecs.defer([this] { _scenes.back(); });
        }
        return;
    }
    const auto focusedIt =
        std::ranges::find_if(list, [this](ecs::entity_t entity) { return _focused.has(entity); });
    const bool appeared = focusedIt == list.end();
    if (appeared) {
        setFocus(list, list.front());
    }
    const render::Vector2 pointer = _metrics.windowToGame({mouse.x, mouse.y});
    const auto hoveredIt =
        std::ranges::find_if(list, [&](ecs::entity_t entity) { return contains(entity, pointer); });
    const std::optional<ecs::entity_t> hovered =
        hoveredIt == list.end() ? std::nullopt : std::optional<ecs::entity_t>(*hoveredIt);

    updatePointerMode(mouseMoved);
    // The pointer only takes the focus when it moves: a still pointer over a widget must not prevent choosing
    // another one with the keyboard
    if (mouseMoved && hovered) {
        setFocus(list, *hovered);
    }
    const auto focusedIndex = [&] {
        return static_cast<std::size_t>(
            std::ranges::find_if(list, [this](ecs::entity_t entity) { return _focused.has(entity); }) -
            list.begin());
    };
    if (_navigation.wasRepeated(Navigation::Up)) {
        setFocus(list, list[(focusedIndex() + list.size() - 1) % list.size()]);
    }
    if (_navigation.wasRepeated(Navigation::Down)) {
        setFocus(list, list[(focusedIndex() + 1) % list.size()]);
    }

    if (_navigation.wasPressed(Navigation::Back)) {
        _ecs.defer([this] { _scenes.back(); });
    } else {
        const ecs::entity_t focused = list[focusedIndex()];
        if (_fields.has(focused)) {
            _fields.get(focused).input.update(_input, elapsed);
        }
        if (_navigation.wasPressed(Navigation::Confirm)) {
            activate(focused);
        }
        if (_navigation.wasRepeated(Navigation::Left)) {
            adjust(focused, -1);
        }
        if (_navigation.wasRepeated(Navigation::Right)) {
            adjust(focused, 1);
        }
        if (hovered && _navigation.wasPressed(Navigation::Click)) {
            setFocus(list, *hovered);
            click(*hovered);
        }
        if (const float wheel = _input.mouseWheel(); hovered && wheel != 0.0f) {
            adjust(*hovered, wheel > 0.0f ? 1 : -1);
        }
    }
    lightPanels(list, hovered, appeared);
    updateSliders(list, hovered, pointer, elapsed);
    // Characters typed while no text field has the focus are lost, not delayed until a field gets it. raylib
    // already drops them every frame; other input sources may keep them, so they are dropped here too.
    (void)_input.takeTypedText();
}

} // namespace rtype::engine
