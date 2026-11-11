# Play in VR

Start the game in a VR headset, on Linux or on Windows. How VR works in the client:
[VR](../architecture/vr.md).

During a game you sit in the cockpit, the game on a board in relief outside its canopy, your ship, the wave, the
other pilots and a radar on the screens of the cockpit, and the menus on a holographic panel in front of you.
You play with the controllers, which point at the panel and vibrate, or with the keyboard or a gamepad. The
cockpit lights up red when you are hit, warns you of a boss, and you hear the other pilots from the side of
their ship.

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

    To start in the headset every time, turn **VR** on in _Settings_, _VR headset_. The same row opens the
    headset at once, without starting the client again, or closes it.

3. **Put the headset on.** The client logs `VR headset open on <runtime>`, then `the headset shows the game`
   once the headset shows it. While the headset shows the game, it sets the pace of the frames, and the window
   ignores its vertical synchronisation and its limit of frames per second. Space and the board fade in from
   black, then the panel unfolds with the main menu. The window shows what the left eye sees, so that the
   people around can follow (_Window_ on the VR page: the left eye, the game flat, or nothing).

4. **Look at the board.** In the menus, the board floats alone in space; once a game starts, you are seated
   in the cockpit, the board just outside its canopy, a little over your eyes. The cockpit is placed from where the head is when the headset starts showing the game: sit as you
   will play before putting the headset on. If you are not in the seat, or the board is not ahead of you,
   hold the **right thumbstick** pushed in for a second, or press **`F9`**, to recentre, looking ahead; recentring with the runtime works too (hold the Meta button on a
   Quest, or the recentre of the SteamVR menu). The cockpit appears a moment after the board, once loaded.
   During a game, the ships, the shots and the bosses come out of the board, as a hologram on a table; the
   planet of the main menu and the hangar stay flat on it.

    !!! tip "A flat board"

        If the game does not keep the pace of the headset, or if you prefer it, set **Board** to _Flat_ on the
        VR page: the game is then drawn once per frame on a flat board, as on a screen, instead of once per eye
        in relief. _Window_ set to _Nothing_ saves a little more.

5. **Start a game.** The cockpit fades in around you, its screens light up one after the other, and the
   canopy glows at your colour for a moment. A hit turns its lights red, with an alarm; low on life, they pulse
   red and the alarm repeats; a boss makes the orange lights pulse; at your death, the screens flicker. The
   cockpit itself never moves.

## The VR page

_Settings_, then _VR headset_, in the _VR_ group. Every choice applies at once, and is kept for the next start.

| Row | What it does |
| --- | ------------ |
| VR | Plays in the headset, now and at each start. Off, the game goes on on the screen. |
| Recentre | Puts the cockpit back around you, the board ahead. |
| Snap turn | Off, 30°, 45° or 90°: the right thumbstick flicked to a side turns you by this much, if you cannot turn your chair. |
| Window | What the window shows: the left eye, the game flat (without the cockpit), or nothing. |
| Board | In relief, or flat for a weaker computer. |
| Distance | How far ahead the board floats, from 1.8 m to 3 m; 2 m by default. |
| Size | How wide the board is, from 2.2 m to 3.3 m; 2.7 m by default. |
| Height | Sits you up to 10 cm higher or lower in the cockpit. |

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
| Right thumbstick | Flicked to a side: snap turn, if chosen | Scroll; flicked to a side: snap turn |
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
  unplugged). The game goes on on the screen; turn **VR** off and on again on the VR page, or start the client
  again, to go back in the headset.
- **The wrong graphics card, on a laptop** with two of them: the runtime draws with the powerful one, and the
  client must run on it too, or the session fails to start or the headset stays black. On Linux, start it with
  `prime-run ./build/bin/r-type_client --xr` (NVIDIA), or with `DRI_PRIME=1` before it (AMD). On Windows, in
  _Settings_, _System_, _Display_, _Graphics_, choose _High performance_ for `r-type_client.exe`; Quest Link
  needs it anyway.
- **The game stutters in the headset**: set **Board** to _Flat_ and **Window** to _Nothing_ on the VR page.
  The `fps` command of the console shows the frames per second; the headset needs 72 to 90.
