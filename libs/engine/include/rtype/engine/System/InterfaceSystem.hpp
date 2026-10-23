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
/// focus. A console text field keeps its size. A widget with a Panel (a button) keeps its size too: its panel
/// lights up instead, fading to the focused look while the pointer is over it, or while it has the focus once
/// the keyboard or a gamepad is used.
/// - The buttons sound (ButtonStyle::hoverSound, clickSound) when their panel lights up and when they are
///   pressed: they are given a SoundEffect, which the SoundSystem plays.
/// - A pressed Flare button bursts (its highlight goes up to FlareStyle::pressedHighlight, then back to 1),
///   and its action runs FlareStyle::pressDelay later, once the burst has been seen.
///
/// The actions of the widgets run after the update (Ecs::defer()): a button may open a menu, which creates
/// entities while this system is still going through the widgets.
///
/// Register it right after the Scenes, before the game systems: the game checks Scenes::receivesInput() for
/// its own scene, so it does not react to keys used by a menu.
class InterfaceSystem : public ecs::System
{
    /// The action of a pressed Flare button, waiting for its burst to be seen.
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
    ecs::ComponentCache<Choice> &_choices;
    ecs::ComponentCache<TextField> &_fields;
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
    [[nodiscard]] float computeFadeDuration(const Panel &panel, float target) const;
    [[nodiscard]] bool isFlare(ecs::entity_t widget) const;
    [[nodiscard]] bool isPressDelayed(ecs::entity_t widget) const;
    void runDelayedPresses(float elapsed);
    void burstPanel(ecs::entity_t widget);
    void playSound(ecs::entity_t widget, const std::optional<SoundEffect> &sound);
    void updatePointerMode(bool mouseMoved);
    void activate(ecs::entity_t widget);
    void click(ecs::entity_t widget);
    void adjust(ecs::entity_t widget, int direction);

public:
    /// @param input, metrics, style They must outlive the system.
    InterfaceSystem(ecs::ComponentCaches &caches, ecs::Ecs &ecs, Scenes &scenes, input::InputSource &input,
                    const Metrics &metrics, const InterfaceStyle &style,
                    input::ActionMap<Navigation> navigation = defaultNavigationControls());

    void update(float elapsed) override;

    /// The navigation controls, to change them in an options menu.
    [[nodiscard]] input::ActionMap<Navigation> &navigation() { return _navigation; }
};

} // namespace rtype::engine
