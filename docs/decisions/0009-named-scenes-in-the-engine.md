# 0009 — Named scenes in the engine

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-09 |

## Context

The `Scenes` system ([ADR 0007](0007-client-in-the-ecs.md)) is a stack of open scenes, identified by the
`SceneId` that `open()` returns. Whoever opens a scene also creates its entities, by hand, wherever it happens
to be (the client's `main`, a menu button). Going from the home screen to the game means knowing every scene
that is open, closing them, then building the game again.

The `Engine` owns the ECS and the events ([ADR 0008](0008-events-switch-systems.md)), but not the scenes: the
client registers `Scenes` itself, and must remember to register it first. `Scenes` also holds a reference to
the whole `Ecs`, only to destroy the entities of the scenes it closes: the stack of scenes depends on the ECS. Events and scenes do not know each
other, although a scene change is when systems most often need to be switched on and off (the game's systems
outside the game).

## Options

| | A. Engine only owns `Scenes` | B. Named scenes in the engine |
|---|---|---|
| Opening a scene | `open()`, then create its entities by hand | By name: the engine calls its build function |
| Changing scene | Close what is open, open the next one, by hand | `switchScene("game")` |
| When it happens | Right away, even inside a system's loop | At the start of the next `Engine::update` |
| Link with events | None | `"<name>:open"` and `"<name>:close"` |

## Decision

**B: the engine registers `Scenes` first, and keeps a table of named scenes.**

- **`Scenes` only keeps the stack**, and no longer knows the `Ecs`: `close()` and `clear()` remove scenes from
  the stack and record their ids. At the start of each update, the engine takes these ids
  (`takeClosedScenes()`) and destroys the entities of those scenes, whoever closed them (a scene request, a
  menu button, Back). Until then, these entities are neither visible nor updated, since their scene is closed.

- `createScene(name, settings, build)` declares a scene once. `build(engine, id)` is called each time it opens
  and creates its entities, each with a `SceneMember`.
- `switchScene(name)` closes every open scene, then opens this one (home screen → game, game over → home
  screen). `pushScene(name)` opens one over the others (pause menu), `popScene()` closes the top one.
- **The requests take effect at the start of the next `Engine::update`**, like a trigger: no entity is
  destroyed while a system goes through them, and a button can switch scene from its callback.
- **Each named scene has two events**, `"<name>:open"` and `"<name>:close"`, created with it. The engine
  triggers them when it opens or closes the scene; their reactions apply in the same update, before the systems
  run. A named scene closed without the engine (`Scenes::close()` from a menu button, Back) triggers its close
  event at the start of the next update.

```cpp
void Engine::update(float elapsed)
{
    applySceneRequests();    // closes and opens scenes, triggers their events
    applyPendingReactions();
    _ecs.update(elapsed);
    restoreOnceReactions();
}
```

## Consequences

- Scenes are declared in one place, and a scene change is a single call, safe from anywhere.
- Systems that belong to a scene subscribe to its events instead of checking which scene is open.
- `Scenes` keeps working on its own: unnamed scenes (the menus of `MenuBuilder`) can still be opened with
  `getScenes().open()`, and are closed by `switchScene` and `popScene` like the others.
- A switch has one frame of delay, like a trigger.
- `Scenes` used without an `Engine` no longer destroys anything: the client and the tests go through the
  `Engine`. The entities of a closed scene live until the next update, hidden.
