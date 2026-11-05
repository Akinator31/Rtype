#pragma once

#include "rtype/input/InputSource.hpp"

#include <string>

namespace rtype::render
{
class Window;
} // namespace rtype::render

namespace rtype::input
{

/// The real keyboard and gamepads.
///
/// Their state is updated once per frame, when the frame is shown (Renderer::endFrame()): every query made
/// during a frame sees the same state.
class Devices : public InputSource
{
public:
    /// @param window The open window, which receives the keyboard events. It must outlive the devices.
    explicit Devices(const render::Window &window);

    [[nodiscard]] bool isKeyDown(Key key) const override;
    [[nodiscard]] bool isGamepadButtonDown(int gamepad, GamepadButton button) const override;
    [[nodiscard]] float gamepadAxis(int gamepad, GamepadAxis axis) const override;
    [[nodiscard]] MousePosition mousePosition() const override;
    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const override;
    [[nodiscard]] float mouseWheel() const override;
    [[nodiscard]] std::u32string takeTypedText() override;
    [[nodiscard]] std::string clipboardText() const override;
    void setClipboardText(const std::string &text) override;
};

/// The name of a key on the keyboard of the player, as printed on it: "Z" for Key::W on a French (AZERTY)
/// keyboard. Empty for a key that prints no character, such as Space or the arrows: name those yourself. A
/// window must be open.
[[nodiscard]] std::string keyLabel(Key key);

} // namespace rtype::input
