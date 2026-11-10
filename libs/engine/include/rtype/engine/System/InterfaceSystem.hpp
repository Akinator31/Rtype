#pragma once

#include "rtype/ecs/Component/ComponentCache.hpp"
#include "rtype/ecs/Component/ComponentCaches.hpp"
#include "rtype/ecs/Hierarchy.hpp"
#include "rtype/ecs/System/System.hpp"
#include "rtype/engine/Component/Animation.hpp"
#include "rtype/engine/Component/Drawing.hpp"
#include "rtype/engine/Component/Sound.hpp"
#include "rtype/engine/Component/Widget.hpp"
#include "rtype/engine/Interface/Metrics.hpp"
#include "rtype/engine/Interface/Navigation.hpp"
#include "rtype/engine/Interface/Style.hpp"
#include "rtype/engine/Scene/Scenes.hpp"
#include "rtype/input/InputSource.hpp"

#include <functional>
#include <optional>
#include <vector>

namespace rtype::engine
{

/// Lets the player use the widgets of the scene that receives the input: focus, navigation with the keyboard,
/// a gamepad or the mouse, and activation.
///
/// - Up and Down move the focus by Focusable order, around at the ends; the mouse focuses the widget under it
///   when it moves.
/// - Confirm activates the focused widget, Left and Right change its value, a click activates the widget
/// under
///   the pointer, the wheel changes its value.
/// - Back goes back from the scene (Scenes::back()).
/// - The focused widget bounces to InterfaceStyle::focusScale, and goes back to its size when it loses the
/// focus. A console text field keeps its size. A widget with a Panel (a button) keeps its size too: its line
/// of light traces itself instead, fading to the lit look while the pointer is over it, or while it has the
/// focus once the keyboard or a gamepad is used.
/// - The buttons sound (ButtonStyle::hoverSound, clickSound) when their panel lights up and when they are
///   pressed: they are given a SoundEffect, which the SoundSystem plays. The sliders and switches with a
///   track make the same sounds when they light up, when a slider is pressed and when a switch changes.
/// - A slider with a SliderTrack is dragged with the mouse: pressing it sets the value under the pointer,
/// which
///   follows the pointer until the button is released. Its track is lit while it is pointed at or has the
///   focus, and grows while it is dragged.
/// - A switch (a Toggle with a SwitchTrack) lights up while the pointer is over its "OFF - ON" part, and its
///   dot slides to the side that is on. Clicking anywhere on its row, Confirm, Left and Right set it, as for
///   every Toggle.
/// - A SettingRow with a `change` goes to its next value on Confirm and a click, and to the next or previous
///   one with Right, Left and the wheel, sounding as a pressed button. It lights up as a button.
/// - A field of a form (TextFieldType::Line or Code) lights up while it has the focus, and keeps its size.
///   Its error is cleared when its content changes; a code is submitted as soon as it is full.
/// - A pressed button bursts (its highlight goes up to FlareStyle::pressedHighlight, then back to 1),
///   and its action runs FlareStyle::pressDelay later, once the burst has been seen.
///
/// The actions of the widgets run after the update (Ecs::defer()): a button may open a menu, which creates
/// entities while this system is still going through the widgets.
///
/// Register it right after the Scenes, before the game systems: the game checks Scenes::receivesInput() for
/// its own scene, so it does not react to keys used by a menu.
class InterfaceSystem : public ecs::System
{
    /// The action of a pressed button, waiting for its burst to be seen.
    struct DelayedPress {
        ecs::entity_t widget;
        float delay;
        std::function<void()> action;
    };

    ecs::Ecs &_ecs;
    Scenes &_scenes;
    input::InputSource &_input;
    const Metrics &_metrics;
    const InterfaceStyle &_style;
    input::ActionMap<Navigation> _navigation;
    render::Vector2 _previousMouse{-1.0f, -1.0f};
    /// Whether the player uses the mouse rather than the keyboard or a gamepad: the panel under the pointer
    /// lights up, instead of the one that has the focus.
    bool _pointerMode = true;
    std::vector<DelayedPress> _delayedPresses;
    /// The slider whose handle the player is dragging with the mouse.
    std::optional<ecs::entity_t> _dragged;
    /// The widget under the pointer at the last update.
    std::optional<ecs::entity_t> _hovered;

    ecs::ComponentCache<Focusable> &_focusables;
    ecs::ComponentCache<Focused> &_focused;
    ecs::ComponentCache<Hitbox> &_hitboxes;
    ecs::ComponentCache<ecs::Position> &_positions;
    ecs::ComponentCache<Scale> &_scales;
    ecs::ComponentCache<Tween> &_tweens;
    ecs::ComponentCache<Panel> &_panels;
    ecs::ComponentCache<Button> &_buttons;
    ecs::ComponentCache<Toggle> &_toggles;
    ecs::ComponentCache<Slider> &_sliders;
    ecs::ComponentCache<SliderTrack> &_tracks;
    ecs::ComponentCache<SwitchTrack> &_switches;
    ecs::ComponentCache<Text> &_texts;
    ecs::ComponentCache<Choice> &_choices;
    ecs::ComponentCache<TextField> &_fields;
    ecs::ComponentCache<SettingRow> &_settingRows;
    ecs::ComponentCache<SoundEffect> &_sounds;

    [[nodiscard]] std::vector<ecs::entity_t> widgets();
    [[nodiscard]] bool contains(ecs::entity_t widget, render::Vector2 point) const;
    void setFocus(const std::vector<ecs::entity_t> &widgets, ecs::entity_t widget);
    [[nodiscard]] bool keepsSize(ecs::entity_t widget) const;
    void animateFocus(ecs::entity_t widget);
    void animateUnfocus(ecs::entity_t widget);
    [[nodiscard]] bool isLit(ecs::entity_t widget, std::optional<ecs::entity_t> hovered) const;
    /// @param appeared The widgets have just appeared: their panels light up silently.
    void lightPanels(const std::vector<ecs::entity_t> &widgets, std::optional<ecs::entity_t> hovered,
                     bool appeared);
    [[nodiscard]] float computeFadeDuration(float target) const;
    [[nodiscard]] bool isPressDelayed(ecs::entity_t widget) const;
    void runDelayedPresses(float elapsed);
    void burstPanel(ecs::entity_t widget);
    void playSound(ecs::entity_t widget, const std::optional<SoundEffect> &sound);
    /// @param wasLit Whether the slider or switch was lit, updated to `lit`.
    void playLightingSound(ecs::entity_t widget, bool &wasLit, bool lit, bool appeared);
    void playSwitchSound(ecs::entity_t widget, bool value);
    void updatePointerMode(bool mouseMoved);
    void activate(ecs::entity_t widget);
    void click(ecs::entity_t widget);
    /// Gives the keys typed to the focused text field. A field of a form loses its error when its content
    /// changes; a code is submitted once full.
    void editField(ecs::entity_t widget, float elapsed);
    void adjust(ecs::entity_t widget, int direction);
    /// Lights the handle of the slider under the pointer, the caption of the sliders and switches under it or
    /// with the focus, slides the dots of the switches, lights the fields of a form that have the focus, and
    /// moves the handle of the slider being dragged.
    void updateSliders(const std::vector<ecs::entity_t> &widgets, std::optional<ecs::entity_t> hovered,
                       render::Vector2 pointer, float elapsed, bool appeared);
    /// Whether the pointer is over the handle of a slider, or over the switch of a toggle with a SwitchTrack.
    [[nodiscard]] bool isOverHandle(ecs::entity_t widget, render::Vector2 pointer) const;
    [[nodiscard]] bool isOverSwitch(ecs::entity_t widget, render::Vector2 pointer) const;

public:
    /// @param input, metrics, style They must outlive the system.
    InterfaceSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, Scenes &scenes, input::InputSource &input,
                    const Metrics &metrics, const InterfaceStyle &style,
                    input::ActionMap<Navigation> navigation = defaultNavigationControls());

    void update(float elapsed) override;

    /// The widget under the pointer at the last update, if any: a VR controller that points at it lights its
    /// ray, and ticks when it reaches another one.
    [[nodiscard]] std::optional<ecs::entity_t> hovered() const { return _hovered; }

    /// The navigation controls, to change them in an options menu.
    [[nodiscard]] input::ActionMap<Navigation> &navigation() { return _navigation; }
};

} // namespace rtype::engine
