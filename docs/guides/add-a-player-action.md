# How to add a player action

This guide adds a new action to the player's controls, for instance a charged shot, binds it to keys and
gamepad buttons, and reacts to it in the client. It is for anyone adding gameplay controls.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- You know how actions work: see [Input](../architecture/input.md).

## Steps

1. **Add the action** to the `Action` enum in `apps/client/src/Controls.hpp`:

    ```cpp
    enum class Action {
        MoveUp,
        MoveDown,
        MoveLeft,
        MoveRight,
        Shoot,
        ChargedShot,   // new
    };
    ```

2. **Bind it** in `defaultControls()`, in `apps/client/src/Controls.cpp`. Give it at least one keyboard key
   and one gamepad input, so that it can be played with both:

    ```cpp
    controls.bind(Action::ChargedShot, Binding::key(Key::LeftShift));
    controls.bind(Action::ChargedShot, Binding::gamepadButton(GamepadButton::FaceRight));
    ```

    Remember that keys are physical positions named after a QWERTY keyboard: `Key::Q` is the `A` key on
    AZERTY. Check that the new keys do not conflict with other actions.

3. **React to it** in the client, after `controls.update(devices)`. Choose the function matching the
   behaviour you want:

    ```cpp
    if (controls.wasPressed(Action::ChargedShot)) {
        // start charging: true for one frame when the key goes down
    }
    if (controls.wasReleased(Action::ChargedShot)) {
        // fire: true for one frame when the key goes up
    }
    ```

    Use `isActive()` for something that lasts while the input is held, and `strength()` for an analog
    value between 0 and 1.

4. **Update the documentation**: add the action to the table of default controls in
   [Input](../architecture/input.md#controls-of-r-type).

5. **Build and run** the client:

    ```sh
    cmake --build build
    ./build/bin/r-type_client
    ```

## Result

The action reacts to every input bound to it. To test logic that depends on actions without a keyboard,
use a fake `InputSource` as in `tests/action_map.cpp`.
