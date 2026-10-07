#pragma once

#include "rtype/input/Binding.hpp"
#include "rtype/input/InputSource.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace rtype::input
{

/// Turns the state of the input devices into game actions, through rebindable bindings.
///
/// The game defines its own actions, usually as an enum, binds each of them to one or more inputs, and only
/// reads actions afterwards: the same code then works with a keyboard, a gamepad, or any other InputSource.
/// @code
/// enum class Action { MoveUp, Shoot };
///
/// ActionMap<Action> actions;
/// actions.bind(Action::Shoot, Binding::key(Key::Space));
/// actions.bind(Action::Shoot, Binding::gamepadButton(GamepadButton::FaceDown));
///
/// // Once per frame, with the time elapsed since the previous frame
/// actions.update(devices, frameTime);
/// if (actions.wasPressed(Action::Shoot)) { ... }
/// @endcode
/// @tparam Action Type identifying the actions, usually an enum class.
template <typename Action>
class ActionMap
{
private:
    struct State {
        std::vector<Binding> bindings;
        float strength = 0.0f;
        bool active = false;
        bool wasActive = false;
        float heldDuration = 0.0f;
        bool repeated = false;
    };

    std::unordered_map<Action, State> _actions;
    float _deadZone = 0.25f;
    float _repeatDelay = 0.4f;
    float _repeatInterval = 0.1f;

    // Number of repeats that happened after holding for this long: -1 before the first one. The small
    // tolerance keeps a repeat that falls exactly on a frame: in float, (0.5 - 0.4) / 0.1 gives 0.9999999
    [[nodiscard]] float repeatCount(float heldDuration) const
    {
        constexpr float tolerance = 1e-4f;
        if (heldDuration + tolerance < _repeatDelay) {
            return -1.0f;
        }
        return std::floor((heldDuration - _repeatDelay) / _repeatInterval + tolerance);
    }

    [[nodiscard]] const State *find(Action action) const
    {
        const auto it = _actions.find(action);
        return it == _actions.end() ? nullptr : &it->second;
    }

public:
    /// Adds an input that triggers the action. Adding the same input twice has no effect.
    void bind(Action action, const Binding &binding)
    {
        std::vector<Binding> &bindings = _actions[action].bindings;
        if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end()) {
            bindings.push_back(binding);
        }
    }

    /// Removes an input from the action. Does nothing if it was not bound to it.
    void unbind(Action action, const Binding &binding)
    {
        if (const auto it = _actions.find(action); it != _actions.end()) {
            std::erase(it->second.bindings, binding);
        }
    }

    /// Removes every input of the action, for instance before the player chooses new ones.
    void clear(Action action)
    {
        if (const auto it = _actions.find(action); it != _actions.end()) {
            it->second.bindings.clear();
        }
    }

    /// Inputs that trigger the action, for instance to show them in the options menu.
    [[nodiscard]] std::vector<Binding> bindings(Action action) const
    {
        const State *state = find(action);
        return state == nullptr ? std::vector<Binding>{} : state->bindings;
    }

    /// Sets the part of the stick and trigger range that is ignored, 0.25 by default. Worn sticks that do
    /// not come back to the centre need a larger value. Clamped between 0 and 0.95.
    void setDeadZone(float deadZone) { _deadZone = std::clamp(deadZone, 0.0f, 0.95f); }

    /// Sets how held actions repeat (see wasRepeated()): first repeat after `delay` seconds, then one every
    /// `interval` seconds. 0.4 s and 0.1 s by default, like text editors. The interval is at least 0.01 s.
    void setRepeat(float delay, float interval)
    {
        _repeatDelay = std::max(delay, 0.0f);
        _repeatInterval = std::max(interval, 0.01f);
    }

    /// Reads the input source and updates every action. Call it once per frame, before reading the actions:
    /// they keep the state read here until the next update.
    /// @param elapsed Time since the previous update, in seconds. It is only needed by heldDuration() and
    /// wasRepeated(): without it, actions are never repeated.
    void update(const InputSource &source, float elapsed = 0.0f)
    {
        for (auto &[action, state] : _actions) {
            state.wasActive = state.active;
            state.strength = 0.0f;
            for (const Binding &binding : state.bindings) {
                state.strength = std::max(state.strength, binding.strength(source, _deadZone));
            }
            state.active = state.strength > 0.0f;

            if (!state.active) {
                // On the update where it is released, the action keeps how long it was held
                if (!state.wasActive) {
                    state.heldDuration = 0.0f;
                }
                state.repeated = false;
            } else if (!state.wasActive) {
                // Just pressed: the press itself counts as the first repeat
                state.heldDuration = 0.0f;
                state.repeated = true;
            } else {
                const float before = state.heldDuration;
                state.heldDuration += elapsed;
                state.repeated = repeatCount(state.heldDuration) > repeatCount(before);
            }
        }
    }

    /// How strongly the action is held, from 0 to 1: the strongest of its inputs. Useful for analog
    /// movement, where a stick pushed halfway moves at half speed.
    [[nodiscard]] float strength(Action action) const
    {
        const State *state = find(action);
        return state == nullptr ? 0.0f : state->strength;
    }

    /// Tells whether the action is held.
    [[nodiscard]] bool isActive(Action action) const
    {
        const State *state = find(action);
        return state != nullptr && state->active;
    }

    /// Tells whether the action started at the last update: true for one frame only, for instance to fire
    /// once per press or to validate a menu.
    [[nodiscard]] bool wasPressed(Action action) const
    {
        const State *state = find(action);
        return state != nullptr && state->active && !state->wasActive;
    }

    /// Tells whether the action was pressed or repeated at the last update: true when it is pressed, then
    /// again at regular intervals while it is held (see setRepeat()). For instance, holding Down in a menu
    /// moves the selection once, then keeps moving it after a short delay.
    [[nodiscard]] bool wasRepeated(Action action) const
    {
        const State *state = find(action);
        return state != nullptr && state->repeated;
    }

    /// How long the action has been held, in seconds: 0 when it was just pressed, and when it is not held.
    /// On the update where it is released (wasReleased()), it still gives how long it was held, so that a
    /// shot charged while holding the button can be fired on release.
    [[nodiscard]] float heldDuration(Action action) const
    {
        const State *state = find(action);
        return state == nullptr ? 0.0f : state->heldDuration;
    }

    /// Tells whether the action stopped at the last update: true for one frame only.
    [[nodiscard]] bool wasReleased(Action action) const
    {
        const State *state = find(action);
        return state != nullptr && !state->active && state->wasActive;
    }
};

} // namespace rtype::input
