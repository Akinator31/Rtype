#pragma once

#include "rtype/input/Devices.hpp"
#include "rtype/render/RenderCommands.hpp"

#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace rtype::client
{

struct InputFrame {
    static constexpr std::size_t gamepads = 4;
    static constexpr std::size_t keys = static_cast<std::size_t>(input::Key::F12) + 1;
    static constexpr std::size_t buttons = static_cast<std::size_t>(input::GamepadButton::RightThumb) + 1;
    static constexpr std::size_t axes = static_cast<std::size_t>(input::GamepadAxis::RightTrigger) + 1;
    static constexpr std::size_t mouseButtons = static_cast<std::size_t>(input::MouseButton::Forward) + 1;

    std::array<bool, keys> keyboard{};
    std::array<std::array<bool, buttons>, gamepads> padButtons{};
    std::array<std::array<float, axes>, gamepads> padAxes{};
    std::array<bool, mouseButtons> mouse{};
    input::MousePosition pointer{};
    float wheel = 0.0f;
    float elapsed = 0.0f;
    std::u32string typed;
    std::string clipboard;

    static InputFrame capture(input::Devices &devices, float elapsed)
    {
        InputFrame frame;
        frame.elapsed = elapsed;
        for (std::size_t index = 0; index < keys; ++index) {
            frame.keyboard[index] = devices.isKeyDown(static_cast<input::Key>(index));
        }
        for (std::size_t pad = 0; pad < gamepads; ++pad) {
            for (std::size_t index = 0; index < buttons; ++index) {
                frame.padButtons[pad][index] = devices.isGamepadButtonDown(
                    static_cast<int>(pad), static_cast<input::GamepadButton>(index));
            }
            for (std::size_t index = 0; index < axes; ++index) {
                frame.padAxes[pad][index] =
                    devices.gamepadAxis(static_cast<int>(pad), static_cast<input::GamepadAxis>(index));
            }
        }
        for (std::size_t index = 0; index < mouseButtons; ++index) {
            frame.mouse[index] = devices.isMouseButtonDown(static_cast<input::MouseButton>(index));
        }
        frame.pointer = devices.mousePosition();
        frame.wheel = devices.mouseWheel();
        frame.typed = devices.takeTypedText();
        frame.clipboard = devices.clipboardText();
        return frame;
    }
};

class InputMailbox
{
    std::mutex _mutex;
    std::condition_variable _changed;
    std::optional<InputFrame> _frame;
    bool _stopped = false;

public:
    void publish(InputFrame frame)
    {
        {
            std::lock_guard lock(_mutex);
            if (_stopped) {
                return;
            }
            if (_frame) {
                frame.typed = _frame->typed + frame.typed;
                frame.wheel += _frame->wheel;
                frame.elapsed += _frame->elapsed;
                for (std::size_t index = 0; index < InputFrame::keys; ++index) {
                    frame.keyboard[index] = frame.keyboard[index] || _frame->keyboard[index];
                }
                for (std::size_t pad = 0; pad < InputFrame::gamepads; ++pad) {
                    for (std::size_t index = 0; index < InputFrame::buttons; ++index) {
                        frame.padButtons[pad][index] =
                            frame.padButtons[pad][index] || _frame->padButtons[pad][index];
                    }
                }
                for (std::size_t index = 0; index < InputFrame::mouseButtons; ++index) {
                    frame.mouse[index] = frame.mouse[index] || _frame->mouse[index];
                }
            }
            _frame = std::move(frame);
        }
        _changed.notify_one();
    }

    [[nodiscard]] std::optional<InputFrame> take()
    {
        std::unique_lock lock(_mutex);
        _changed.wait(lock, [this] { return _frame.has_value() || _stopped; });
        if (!_frame) {
            return std::nullopt;
        }
        return std::exchange(_frame, std::nullopt);
    }

    void stop()
    {
        {
            std::lock_guard lock(_mutex);
            _stopped = true;
            _frame.reset();
        }
        _changed.notify_all();
    }
};

class SnapshotInput : public input::InputSource
{
    InputFrame _frame;
    std::function<void(const std::string &)> _copy;

public:
    explicit SnapshotInput(std::function<void(const std::string &)> copy) : _copy(std::move(copy)) {}

    void update(InputFrame frame) { _frame = std::move(frame); }

    [[nodiscard]] bool isKeyDown(input::Key key) const override
    {
        return _frame.keyboard[static_cast<std::size_t>(key)];
    }
    [[nodiscard]] bool isGamepadButtonDown(int gamepad, input::GamepadButton button) const override
    {
        return gamepad >= 0 && static_cast<std::size_t>(gamepad) < InputFrame::gamepads &&
               _frame.padButtons[static_cast<std::size_t>(gamepad)][static_cast<std::size_t>(button)];
    }
    [[nodiscard]] float gamepadAxis(int gamepad, input::GamepadAxis axis) const override
    {
        return gamepad >= 0 && static_cast<std::size_t>(gamepad) < InputFrame::gamepads
                   ? _frame.padAxes[static_cast<std::size_t>(gamepad)][static_cast<std::size_t>(axis)]
                   : 0.0f;
    }
    [[nodiscard]] input::MousePosition mousePosition() const override { return _frame.pointer; }
    [[nodiscard]] bool isMouseButtonDown(input::MouseButton button) const override
    {
        return _frame.mouse[static_cast<std::size_t>(button)];
    }
    [[nodiscard]] float mouseWheel() const override { return _frame.wheel; }
    [[nodiscard]] std::u32string takeTypedText() override { return std::exchange(_frame.typed, {}); }
    [[nodiscard]] std::string clipboardText() const override { return _frame.clipboard; }
    void setClipboardText(const std::string &text) override { _copy(text); }
};

} // namespace rtype::client
