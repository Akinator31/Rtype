#pragma once

#include <cstdint>

namespace rtype::game
{

/// What one player does during one tick: the controls held, one bit each, exactly the byte of the protocol
/// (docs/protocol/game.md, section 9.2).
///
/// It is data, not a device: the server receives it from the network, and the client makes it from the
/// virtual gamepad of the engine (rtype::engine::Controls, fed by libs/input), so that the rules depend on
/// neither raylib nor the bindings:
/// @code
/// rtype::game::PlayerInput input;
/// input.set(rtype::game::PlayerInput::Up, controls.isDown(rtype::engine::Control::Up));
/// input.set(rtype::game::PlayerInput::Fire, controls.isDown(rtype::engine::Control::FaceDown));
/// simulation.setInput(slot, input);
/// @endcode
struct PlayerInput {
    /// The bits of the controls.
    enum Button : std::uint8_t {
        Up = 1 << 0,
        Down = 1 << 1,
        Left = 1 << 2,
        Right = 1 << 3,
        /// Held: the game decides what holding it does (shoot again, later charge a shot).
        Fire = 1 << 4,
        /// Held: the ship turns its nose up, or down, by AimAngle, and fires that way.
        AimUp = 1 << 5,
        AimDown = 1 << 6,
    };
    /// The bits the protocol reserves: ignored.
    static constexpr std::uint8_t Reserved = 0x80;

    /// The bits of the buttons held.
    std::uint8_t buttons = 0;

    /// The input of a byte received: the reserved bits are dropped.
    [[nodiscard]] static PlayerInput fromByte(std::uint8_t byte)
    {
        return {static_cast<std::uint8_t>(byte & ~Reserved)};
    }

    [[nodiscard]] bool isHeld(Button button) const { return (buttons & button) != 0; }

    void set(Button button, bool held)
    {
        buttons =
            held ? static_cast<std::uint8_t>(buttons | button) : static_cast<std::uint8_t>(buttons & ~button);
    }

    /// The direction the player asks for, each coordinate -1, 0 or 1: up and down together, or left and right
    /// together, cancel each other.
    struct Direction {
        int x = 0;
        int y = 0;
    };
    [[nodiscard]] Direction direction() const;

    /// Where the player turns its ship: -1 nose up, 1 nose down, 0 straight ahead; both together cancel each
    /// other.
    [[nodiscard]] int aim() const;

    friend bool operator==(const PlayerInput &, const PlayerInput &) = default;
};

} // namespace rtype::game
