#pragma once

#include "rtype/input/Binding.hpp"
#include "rtype/input/InputSource.hpp"

#include <algorithm>
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
/// // Once per frame
/// actions.update(devices);
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
    };

    std::unordered_map<Action, State> _actions;
    float _deadZone = 0.25f;

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

    /// Reads the input source and updates every action. Call it once per frame, before reading the actions:
    /// they keep the state read here until the next update.
    void update(const InputSource &source)
    {
        for (auto &[action, state] : _actions) {
            state.wasActive = state.active;
            state.strength = 0.0f;
            for (const Binding &binding : state.bindings) {
                state.strength = std::max(state.strength, binding.strength(source, _deadZone));
            }
            state.active = state.strength > 0.0f;
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

    /// Tells whether the action stopped at the last update: true for one frame only.
    [[nodiscard]] bool wasReleased(Action action) const
    {
        const State *state = find(action);
        return state != nullptr && !state->active && state->wasActive;
    }
};

} // namespace rtype::input
