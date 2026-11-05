# How to add a menu

This guide adds a menu to the client, for instance a *Controls* menu opened from the options. It is for
anyone building the interface.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- You know how menus work: see [Menus](../architecture/scenes-and-interface.md#menus) and
  [Scenes](../architecture/scenes-and-interface.md#scenes).

## Steps

1. **Declare a function that opens the menu** in `apps/client/src/Menus.hpp`, next to the others. It takes the
   `MenuTargets`: what the menus are built with and act on.

    ```cpp
    /// Opens the controls menu: the keys of each action.
    void openControlsMenu(const MenuTargets &targets);
    ```

2. **Build the menu** in `apps/client/src/Menus.cpp` with a `MenuBuilder`. It opens a scene, and creates a
   background, a title and the widgets stacked at the centre.

    ```cpp
    void openControlsMenu(const MenuTargets &targets)
    {
        engine::MenuBuilder menu(targets.ecs, targets.scenes, targets.metrics, targets.style, "Controls");
        const engine::SceneId scene = menu.scene();

        menu.label("Choose an action to change its keys");
        menu.button("Shoot", [] { /* ... */ });
        menu.button("Back", [targets, scene] { targets.scenes.close(scene); });
    }
    ```

    - **Overlay or not**: pass `{.overlay = true}` as the last argument of the `MenuBuilder` to keep the scene
      below visible, dimmed, like the pause menu over the game. Without it, the menu hides what is below, like
      the options.
    - **Widgets**: `label`, `button`, `toggle`, `slider`, `choice`, `textField` and `passwordField` (see the table in
      [Menus](../architecture/scenes-and-interface.md#menus)). Settings are given as functions that read and
      write them, never as copies.
    - **Captures**: the lambdas capture `targets` **by copy** (it only holds references) and the scene id.
      Never capture local variables by reference: the function returns long before the buttons are pressed.

3. **Open it** from another menu, or from the game:

    ```cpp
    options.button("Controls", [targets] { openControlsMenu(targets); });
    ```

    From the game code, open a menu only while the game scene receives the input
    (`scenes.receivesInput(game)`), as the pause menu is opened in `main.cpp`.

4. **Going back.** Escape (or the right face button of a gamepad) closes the menu at the top and shows the
   one below. To do something else, for instance asking for confirmation before leaving the game, give an
   `onBack` to the scene: `{.onBack = [targets] { openQuitConfirmation(targets); }}`.

5. **Test it** without a window, like `tests/engine.cpp`: build the menu with the `FakeInput` and fake text
   measures, press keys, and check what the widgets did.

## Result

The menu opens over the others, can be used with the keyboard, a gamepad or the mouse, and its entities are
destroyed when it closes. The focused widget bounces, like in every menu built with `MenuBuilder`.
