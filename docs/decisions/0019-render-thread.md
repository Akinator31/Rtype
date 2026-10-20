# 0019 — Render thread and frame hand-off

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-20 |

## Context

The client used to update the ECS, read input, measure interface text and draw the scene on one thread. Rendering can block while waiting for the display or GPU. The window and OpenGL context also impose an owner thread: moving raylib drawing to a worker while the main thread polls the same window would split ownership of raylib state.

## Decision

The main thread owns the window, raylib input devices, renderer, OpenGL context and resource uploads. A separate logic thread owns the ECS, scenes, network loop and game systems. It records each frame as an ordered list of draw commands whose arguments are copied values and resource handles. The main thread replays those commands. The recorded frame contains no entity or component references, so the ECS can change after publication.

The frame hand-off has one pending slot alongside the frame currently displayed. A new complete frame replaces an older pending frame if rendering falls behind. The renderer can redisplay the last complete frame while the logic thread builds the next one. This bounds memory and avoids a growing frame queue. Model animation commands stay in draw order so shared models are posed immediately before each draw.

Raylib measurements needed by interface layout, such as text width and the visible area, are submitted as synchronous queries to the main thread. The main thread drains these queries before drawing. The logic thread never calls raylib through `RendererMetrics` or `SceneRenderer`. The client preloads its current game and menu graphics before starting logic. A synchronous cache miss on the logic thread is rejected by `ResourceManager`; new graphics must use its asynchronous API and be finalized on the main thread.

After each displayed frame, the main thread copies keyboard, gamepad, mouse, text and clipboard state into an input mailbox. The logic thread consumes that copy. When several render frames arrive before a logic update, the mailbox combines elapsed time, wheel motion, typed characters and button presses while retaining the latest pointer and axis positions. Clipboard writes are dispatched back to the main thread. `Window::requestClose()` uses an atomic flag so the logic thread can request shutdown without calling raylib.

On shutdown, the main thread stops both mailboxes and pending measurement queries, then joins the logic thread before destroying the renderer, resources and window. A render error follows the same path. Tests cover query thread ownership, frame replacement and input event merging.

## Consequences

- Raylib window creation, polling, drawing and GPU work remain on one thread.
- Logic and rendering do not read the ECS concurrently.
- Interface measurements can temporarily block logic until the render thread services the query.
- The renderer may display the previous complete frame while logic is busy; intermediate complete frames may be skipped.
