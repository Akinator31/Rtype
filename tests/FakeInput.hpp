#pragma once

// Input source whose keys, buttons, axes and typed text are set by the test: no window nor device is needed.

#include "rtype/input/InputSource.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>

class FakeInput : public rtype::input::InputSource
{
public:
    std::set<rtype::input::Key> keys;
    std::set<std::pair<int, rtype::input::GamepadButton>> buttons;
    std::map<std::pair<int, rtype::input::GamepadAxis>, float> axes;
    /// Characters typed since the previous frame: emptied when read, like the real ones.
    std::u32string typed;
    /// Text of the clipboard.
    std::string clipboard;

    [[nodiscard]] bool isKeyDown(rtype::input::Key key) const override { return keys.contains(key); }
    [[nodiscard]] bool isGamepadButtonDown(int gamepad, rtype::input::GamepadButton button) const override
    {
        return buttons.contains({gamepad, button});
    }
    [[nodiscard]] float gamepadAxis(int gamepad, rtype::input::GamepadAxis axis) const override
    {
        const auto it = axes.find({gamepad, axis});
        return it == axes.end() ? 0.0f : it->second;
    }
    [[nodiscard]] std::u32string takeTypedText() override { return std::exchange(typed, {}); }
    [[nodiscard]] std::string clipboardText() const override { return clipboard; }
    void setClipboardText(const std::string &text) override { clipboard = text; }
};
