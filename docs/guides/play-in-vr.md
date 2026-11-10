# Play in VR

Start the game in a VR headset, on Linux or on Windows. How VR works in the client:
[VR](../architecture/vr.md).

!!! warning "In progress"

    For now you sit in the cockpit during a game, the game on a board in relief outside its canopy, your ship,
    the wave, the other pilots and a radar on the screens of the cockpit, and the menus on a holographic panel
    in front of you. You play with the controllers, which point at the panel and vibrate, or with the keyboard
    or a gamepad. A VR page in the settings comes next.

## What you need

| | Linux | Windows |
|---|---|---|
| Quest 2, 3, 3S, Pro | [WiVRn](https://github.com/WiVRn/WiVRn) (wireless), or ALVR with SteamVR | Quest Link (cable) or Air Link, in the Meta Horizon Link application |
| Index, Vive, other PC headsets | SteamVR, or [Monado](https://monado.freedesktop.org/) | SteamVR |

No other library is needed: the client embeds the OpenXR loader. The client must be built with `RTYPE_OPENXR`
on, its default on Linux and Windows.

## Steps

1. **Start the runtime and connect the headset.** WiVRn: start `wivrn-server`, then the WiVRn application in
   the headset. Quest Link: open the Meta Horizon Link application, connect the headset, and set Meta Horizon
   Link as the active OpenXR runtime in its settings (_General_, _OpenXR Runtime_). SteamVR: start SteamVR, and
   in its settings set it as the active OpenXR runtime.

2. **Start the client with `--xr`:**

    ```sh
    ./build/bin/r-type_client --xr
    ```

    To start in the headset every time, add `vr.enabled=true` to the settings file of the client
    ([Settings](../architecture/settings.md)).

3. **Put the headset on.** The client logs `VR headset open on <runtime>`, then `the headset shows the game`
   once the headset shows it. While the headset shows the game, it sets the pace of the frames, and the window
   ignores its vertical synchronisation and its limit of frames per second. The window shows what the left eye
   sees, so that the people around can follow.

4. **Look at the board.** In the menus, the board floats alone in space; once a game starts, you are seated
   in the cockpit, the board just outside its canopy, a little over your eyes. The cockpit is placed from where the head is when the headset starts showing the game: sit as you
   will play before putting the headset on. If you are not in the seat, or the board is not ahead of you,
   hold the **right thumbstick** pushed in for a second, or press **`F9`**, to recentre, looking ahead; recentring with the runtime works too (hold the Meta button on a
   Quest, or the recentre of the SteamVR menu). The cockpit appears a moment after the board, once loaded.
   During a game, the ships, the shots and the bosses come out of the board, as a hologram on a table; the
   planet of the main menu and the hangar stay flat on it.

    !!! tip "A flat board"

        If the game does not keep the pace of the headset, or if you prefer it, add `vr.flatBoard=true` to the
        settings file: the game is then drawn once per frame on a flat board, as on a screen, instead of once
        per eye in relief.

## The controllers

In the menus, point at the panel and pull the **trigger** to click: the ray of the controller lights up on a
button, and the controller ticks in your hand. Drag a slider with the trigger held, scroll with the **right
thumbstick**. Either hand points: the one whose trigger you pulled last. While the panel is open, a trigger
pulled off the panel does nothing.

| Controller | In a game | In the menus |
| ---------- | --------- | ------------ |
| Left thumbstick | Move | Move between the buttons |
| Right trigger, or A | Fire, held to fire again | Trigger: click; A: confirm |
| Left, right grip | Aim up, aim down | — |
| Left trigger | Push to talk | Click |
| Y | Mute the voice | — |
| B | — | Back |
| Menu | The pause menu | Back |
| Right thumbstick | — | Scroll |
| Right thumbstick held in, 1 s | Recentre | Recentre |

The controllers are a gamepad for the game, with the buttons of a gamepad, and the keyboard and a real gamepad
still work beside them. On a Valve Index, the left thumbstick pushed in is the menu button; on an HTC Vive, the trackpad is the
thumbstick. A shot ticks in your right hand, a hit pulses in both, and your ship destroyed rumbles. While you
play with a real gamepad, the controllers are no longer drawn, until you use one of their buttons.

## When the headset shows nothing

- **"No VR headset found"** in the main menu: the client found no runtime or no headset, and goes on on the
  screen. The OpenXR loader writes what it looked for on the error output of the client:
  - `failed to determine active runtime file path`: no runtime is active. Start it (step 1), or check that it
    wrote `~/.config/openxr/1/active_runtime.json` (Linux), or that it is the active OpenXR runtime (Windows).
  - `sees no headset`: the runtime runs, but the headset is not connected to it.
- **"no current GLX context"** on Linux: the client runs on Wayland directly. It needs X11, through XWayland,
  as raylib builds it by default.
- **"The VR headset stopped"** while playing: the runtime ended the session (it was quit, or the headset was
  unplugged). The game goes on on the screen; start the client again to go back in the headset.
