# Play in VR

Start the game in a VR headset, on Linux or on Windows. How VR works in the client:
[VR](../architecture/vr.md).

!!! warning "In progress"

    For now the headset shows a plain dark blue: the game itself is still on the screen. The cockpit, the
    board and the controllers come next.

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
   ignores its vertical synchronisation and its limit of frames per second.

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
