# 0037 — VR through OpenXR, seated in a cockpit

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-09 |

## Context

The game is played in a VR headset, menus included, on Linux and on Windows, with the headsets the team has
(Quest 2 and 3, through WiVRn on Linux and Quest Link or Air Link on Windows) and the PC headsets of SteamVR. A
proof of concept on raylib ran a glTF model in a Quest on both systems.

The gameplay is a plane: the server, the protocol, the hitboxes and the replays work in game units on it, and
must not change. VR is a way of drawing the client and of reading its input.

The client draws with raylib on OpenGL ([ADR 0002](0002-raylib-graphics-library.md)), in a single window, and
links no other graphics API.

## Options

**The API of the headset**

| | Kept? | Why |
|---|---|---|
| A. **OpenXR**, on the OpenGL context of the window (`XR_KHR_opengl_enable`, WGL or GLX) | **Yes** | The standard of Khronos, every runtime of our headsets has it (WiVRn, Monado, SteamVR, Meta); the loader is Apache 2.0 and builds on both systems; the proof of concept works on it |
| B. The VR simulator of raylib (`LoadVrStereoConfig`) | No | A lens distortion drawn in a window: no headset, no tracking, no controllers |
| C. OpenVR | No | SteamVR only, deprecated by Valve in favour of OpenXR |
| D. A VR engine (Godot, Unity, Unreal) | No | A second engine beside ours, the client rewritten |

**The view of the player**

| | Kept? | Why |
|---|---|---|
| E. First person, from the ship of the player | No | A camera in the plane of the game, looking along it, sees the plane as a line; following a ship that climbs and dives makes players sick |
| F. A board floating in front of the player, nothing around it | No | Comfortable, but a screen in a dark room |
| G. **Seated in a cockpit (`cockpit.glb`), the board outside its canopy** | **Yes** | As comfortable as F, the camera never moving on its own, and the cockpit carries the interface: the HUD on its screens, the menus on a holographic panel |

## Decision

**A and G: an OpenXR session on the OpenGL context of the window, the player seated in a cockpit, and the
game a board outside its canopy.**

- A library of its own, `libs/xr` (namespace `rtype::xr`), since it wraps a dependency: no OpenXR type in its
  headers, and no `raylib.h` in its sources (they clash with `windows.h` and Xlib), only `rlgl.h` for the
  framebuffers of the eyes.
- The space of reference is `LOCAL`, the head of the seated player, not `STAGE`, the floor of a room.
- The loader of OpenXR is built by CPM (OpenXR-SDK 1.1.60) and linked **statically**, on Linux too, rather than
  the one of the system: a client linked to `libopenxr_loader.so` would not start on a computer without it,
  headset or not. The loader still finds the active runtime itself.
- `RTYPE_OPENXR` (on for Linux and Windows) builds it; off, or on macOS, the library has the same interface and
  never opens a headset.
- VR never stops the game: no runtime, no headset, or a session lost, and the client goes on on the screen.
- On Linux the client runs on X11 (XWayland under Wayland), since raylib builds GLFW without Wayland: the GLX
  binding is the one needed. A Wayland build of GLFW would need the EGL binding of OpenXR instead.

## Consequences

- The client takes the OpenXR loader (Apache 2.0) and the jsoncpp it embeds (public domain or MIT); their
  licences go into the package.
- Every frame, the headset waits for its own refresh (`xrWaitFrame`): while it shows the game, the window
  neither synchronises with the screen nor limits its frames per second (`Window::setPacedElsewhere()`).
- The renderer has to draw into the images of the eyes with the matrices of the headset, the interface into
  images placed in the cockpit, and the controllers become an input source: the
  [next steps](../architecture/vr.md#next-steps) of VR.
- The standalone Quest (Android, OpenGL ES) is not covered: it would need another binding and another build.
