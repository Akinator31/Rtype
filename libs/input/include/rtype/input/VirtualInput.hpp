#pragma once

#include "rtype/input/InputSource.hpp"
#include "rtype/input/Keys.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace rtype::input
{

/// The number of buttons of a gamepad, and of its axes: the values of GamepadButton and GamepadAxis.
inline constexpr std::size_t GamepadButtonCount = static_cast<std::size_t>(GamepadButton::RightThumb) + 1;
inline constexpr std::size_t GamepadAxisCount = static_cast<std::size_t>(GamepadAxis::RightTrigger) + 1;

/// What a gamepad that is not a device holds this frame: the controllers of a VR headset, presented as one.
struct GamepadState {
    std::array<bool, GamepadButtonCount> buttons{};
    /// In the ranges of GamepadAxis: sticks from -1 to 1, triggers from 0 to 1.
    std::array<float, GamepadAxisCount> axes{};

    [[nodiscard]] bool &button(GamepadButton which) { return buttons.at(static_cast<std::size_t>(which)); }
    [[nodiscard]] bool button(GamepadButton which) const
    {
        return buttons.at(static_cast<std::size_t>(which));
    }
    [[nodiscard]] float &axis(GamepadAxis which) { return axes.at(static_cast<std::size_t>(which)); }
    [[nodiscard]] float axis(GamepadAxis which) const { return axes.at(static_cast<std::size_t>(which)); }
};

/// What a pointer that is not the mouse gives this frame: a controller pointing at a panel.
struct PointerState {
    /// Where it points, in the units the interface reads the mouse in (engine::Metrics::windowToGame()).
    MousePosition position;
    /// The button that clicks, held.
    bool held = false;
    /// How far it scrolled since the previous frame, in notches of the wheel: positive up.
    float wheel = 0.0f;
};

/// An input source over another one, the devices, with a virtual gamepad and a pointer added: the
/// controllers of a VR headset (docs/architecture/vr.md#the-controllers). Every ActionMap that reads it then
/// works with them without a case of its own, with the bindings of the player.
///
/// - The keyboard, the text typed and the clipboard are those of the source.
/// - The virtual gamepad is gamepad 0, held together with the real one: a button is held when it is held on
///   either, an axis is the one pushed the farthest. The other gamepads are those of the source.
/// - While a pointer is given, it replaces the mouse: its position, its button as the left one, and its
///   scrolling as the wheel. Without one, the mouse is the one of the source.
///
/// @code
/// VirtualInput input(devices);
/// engine.setInputSource(input);
/// // Each frame, before the engine reads the input
/// input.setGamepad(controllersAsGamepad);
/// input.setPointer(handOnPanel ? std::optional(pointer) : std::nullopt);
/// @endcode
class VirtualInput final : public InputSource
{
    InputSource &_source;
    GamepadState _gamepad;
    std::optional<PointerState> _pointer;

public:
    /// @param source The devices, which must outlive this source.
    explicit VirtualInput(InputSource &source) : _source(source) {}

    /// What the virtual gamepad holds from now on: released by default.
    void setGamepad(const GamepadState &gamepad) { _gamepad = gamepad; }
    [[nodiscard]] const GamepadState &gamepad() const { return _gamepad; }
    /// The pointer that replaces the mouse from now on, or none: the mouse of the source again.
    void setPointer(std::optional<PointerState> pointer) { _pointer = pointer; }
    [[nodiscard]] const std::optional<PointerState> &pointer() const { return _pointer; }

    [[nodiscard]] bool isKeyDown(Key key) const override { return _source.isKeyDown(key); }
    [[nodiscard]] bool isGamepadButtonDown(int gamepad, GamepadButton button) const override;
    [[nodiscard]] float gamepadAxis(int gamepad, GamepadAxis axis) const override;
    [[nodiscard]] MousePosition mousePosition() const override;
    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const override;
    [[nodiscard]] float mouseWheel() const override;
    [[nodiscard]] std::u32string takeTypedText() override { return _source.takeTypedText(); }
    [[nodiscard]] std::string clipboardText() const override { return _source.clipboardText(); }
    void setClipboardText(const std::string &text) override { _source.setClipboardText(text); }
};

} // namespace rtype::input
