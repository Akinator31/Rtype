# 0008 — Events switch systems on and off

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-09 |

## Context

The engine (`libs/engine`) has named events: systems subscribe to an event, and something (the input, another
system) triggers it. A first version made `Event::trigger()` call `update()` on every subscribed system, right
away. Since systems take the elapsed time ([ADR 0007](0007-client-in-the-ecs.md)), it had to pass a made-up
`update(0.0f)`, which showed deeper problems:

- `update(elapsed)` meant two things: "a frame went by" when called by the ECS, "something happened" when
  called by an event. A system could not tell them apart, and 0 is also the elapsed time of a paused frame.
- A system is built by `Ecs::registerSystem`, which gives it its component caches, so it also runs every frame:
  a subscribed system was updated twice. Disabling it in the ECS also stopped the event, which skips disabled
  systems too.
- The triggered system ran in the middle of the system that triggered the event, out of the order of the frame
  (scenes first, the interface before the game, positions before drawing). A subscriber removed during a
  trigger changed the list being gone through.

The question was what triggering an event does to the subscribed systems, without changing the ECS, which stays
limited to entities, components and systems.

## Options

| | A. Trigger calls `update(0)` | B. Typed event queues | C. Trigger switches `enabled` |
|---|---|---|---|
| Meaning of `update(elapsed)` | Two meanings, made-up elapsed time | One: a frame | One: a frame |
| When the system runs | Inside the triggering system | At its place in the frame | At its place in the frame |
| Systems run by an event only | Impossible (updated twice) | Run every frame, do nothing without events | Yes: disabled until an event enables them |
| Data carried by the event | None | Typed (`Collision{a, b}`) | None: the data stays in components |
| Code | An event class | A queue per event type, given to the systems that read it | An event class and two lists in `Engine` |
| Systems to write | Unchanged | Each reads its queues | Unchanged: they do not know the events |

B is the classic ECS answer when events carry data. Here an event wakes systems up, and what they need is
already in components (a collision system adds a component, the damage system goes through the entities that
have it), so C keeps the systems unaware of events, with less code.

## Decision

**C: triggering an event changes the `enabled` flag of the subscribed systems.**

Each subscription gives a **reaction** and a **duration**:

| | `Duration::Permanent` | `Duration::Once` |
|---|---|---|
| `Reaction::Enable` | enables the system | enables it for the next update, then restores it |
| `Reaction::Disable` | disables the system | disables it for the next update, then restores it |
| `Reaction::Toggle` | inverts the flag | inverts it for the next update, then restores it |

**A trigger takes effect at the start of the next `Engine::update`**, wherever it comes from (the input in the
main loop, a system, a deferred action):

```cpp
void Engine::update(float elapsed)
{
    applyPendingReactions(); // the triggers received since the last update
    _ecs.update(elapsed);
    restoreOnceReactions();  // every system ran at most once: restore the "once" ones
}
```

Restoring at the end of the update in which the event is triggered would be wrong: a system registered before
the one that triggers the event is already skipped for this frame, then restored before it ever runs. Waiting
for the next update gives every trigger one frame of delay, whatever the order of the systems.

Two rules settle the conflicts:

- **The reactions are applied in the order of the triggers; the last one wins.** Two `Toggle` in the same frame
  cancel each other.
- **A permanent reaction cancels a pending restore.** It is an explicit decision and is not undone. A system
  hit by several `Once` reactions in the same update is restored to its state before the first one.

## Consequences

- Systems keep a single `update(float elapsed)`, run once per frame at their place, with the real elapsed
  time. They do not know about events.
- A system run by an event only is registered disabled, and subscribed with `Reaction::Enable,
  Duration::Once`.
- An event carries no data: what a woken system needs must be in components.
- The frame must go through `Engine::update` instead of `Ecs::update`, or the reactions are never applied. The
  client still updates its `Ecs` directly and has to move to `Engine`.
- The `enabled` flag of a subscribed system belongs to its events: changing it by hand during a frame where a
  `Once` reaction is pending is overwritten by the restore.
- If events need to carry data later (network messages, collisions with their entities), typed queues (option
  B) can be added next to this mechanism; they do not replace it.
