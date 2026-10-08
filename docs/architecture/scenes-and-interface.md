# Scenes and interface

Everything the client draws is an entity of the ECS: the world, the HUD and the menus
([ADR 0007](../decisions/0007-client-in-the-ecs.md)). This page describes the features of the ECS this relies on
(`libs/ecs`, shared with the server) and the client library built on them (`libs/engine`).

## Where things are

The client library is organised like the ECS, with one file per system:

```
libs/engine/include/rtype/engine/
  Component/    the data attached to entities
    Drawing.hpp     Layer, Drawable, Text, Sprite, Box, Scale, Opacity
    Animation.hpp   Easing, ease(), Tween
    Widget.hpp      Focusable, Focused, Hitbox, Button, Toggle, Slider, Choice, TextField
    Layout.hpp      Column, ColumnItem
  System/       the systems, one per file
    InterfaceSystem.hpp  WidgetTextSystem.hpp  TweenSystem.hpp  HitboxSystem.hpp  ColumnLayoutSystem.hpp
  Interface/    what menus are built with
    MenuBuilder.hpp  Navigation.hpp  Style.hpp  Metrics.hpp
  Rendering/
    SceneRenderer.hpp
  Engine.hpp    includes everything above
```

The sources in `libs/engine/src/` follow the same folders. Every header compiles on its own: include the
precise headers you need, or `rtype/engine/Engine.hpp` for everything.

## The systems and their order

```cpp
Ecs ecs;
Scenes &scenes = ecs.registerSystem<Scenes>(ecs);                                    // 1. first
ecs.registerSystem<InterfaceSystem>(ecs, scenes, devices, metrics, style);            // 2. the interface
// 3. the game systems
ecs.registerSystem<WidgetTextSystem>();                                               // 4. state to drawing
ecs.registerSystem<TweenSystem>();
ecs.registerSystem<HitboxSystem>(metrics, style);
ecs.registerSystem<ColumnLayoutSystem>(ecs);
ecs.registerSystem<TransformSystem>(ecs);                                             // 5. positions, last
SceneRenderer sceneRenderer(ecs, scenes, style);                                      // not a system

// Each frame
ecs.update(window.frameTime());
renderer.beginFrame();
sceneRenderer.draw(renderer, Layer::World);
sceneRenderer.draw(renderer, Layer::Interface);
renderer.endFrame();
```

| # | System | Role |
|---|---|---|
| 1 | `Scenes` | Records which scene receives the input for the whole update. |
| 2 | `InterfaceSystem` | Focus and navigation in the menus; reads the input before the game. |
| 3 | Game systems | Movement, shots, network... They check `scenes.receivesInput()` for their scene. |
| 4 | `WidgetTextSystem`, `TweenSystem`, `HitboxSystem`, `ColumnLayoutSystem` | Texts of the widgets, animations, sizes, layout. |
| 5 | `TransformSystem` | Positions of the children from their parents. |

The `SceneRenderer` is not a system: drawing happens after the update, between `beginFrame()` and `endFrame()`,
and may happen several times per frame (once per eye in VR).

## ECS features

### Elapsed time

Systems receive the time elapsed since the previous update: `update(float elapsed)`. They never read a clock,
so that tests and the server control time.

### Entity references

Entity ids are reused once an entity is destroyed. An `EntityRef` (`ecs.ref(entity)`) adds a generation: once
the entity is destroyed, `ecs.isAlive(reference)` is false, even if its id was given to a new entity. Keep
`EntityRef`, not `entity_t`, in a component that points to another entity.

### Deferred actions

`ecs.defer(action)` runs an action after the systems of the update. A system going through the entities must not
create or destroy entities of the types it iterates: the caches could be reallocated under it. A button that
opens a menu, for instance, has its action deferred.

## Hierarchy

| Component | Meaning |
|---|---|
| `Position` | Position in the world, in game units. Computed for entities with a parent. |
| `Parent` | The entity is attached to another one (`EntityRef`). |
| `LocalPosition` | Position relative to the parent. |
| `Hidden` | Hides the entity and its children, without destroying them. |

```cpp
attach(ecs, force, ship, {64.0f, 0.0f});   // the Force follows the ship, 64 units in front of it
```

- **Following**: the `TransformSystem` sets `Position = position of the parent + LocalPosition`, parents first.
- **Hiding**: `isHidden(ecs, entity)` is true if the entity or one of its ancestors has `Hidden`. A closed
  dropdown hides its options this way.
- **Destroying**: `ecs.destroyEntity(boss)` also destroys its turrets, recursively. A child never outlives its
  parent with a dangling reference.

## Scenes

A scene groups the entities of a phase of the game: home screen, game, pause menu, options. Every entity of a
scene has a `SceneMember`; closing the scene destroys them all at once. Entities without it are **persistent**:
a star-field shared by the menus and the game survives every scene change.

```cpp
const SceneId game = scenes.open({.updatesWhenCovered = true});
const entity_t ship = ecs.createEntity();
ecs.addComponent<SceneMember>(ship, game);
```

Scenes form a stack:

| Setting | Effect | Example |
|---|---|---|
| `overlay` | The scenes below stay visible | Pause menu over the game |
| `updatesWhenCovered` | The scene keeps updating under others | Network game under the pause menu |
| `onBack` | Called on Back instead of closing the scene | Game: opens the pause menu |

Systems ask `Scenes` what to do with an entity or a scene: `receivesInput()` (only the scene at the top),
`updates()`, `isVisible()`.

!!! warning "The input owner is set at the start of the update"

    The scene that receives the input is recorded when the update starts, by the `Scenes` system. When Escape
    closes the pause menu, the game does not receive the input until the next update: it does not see the
    same Escape press and does not open the menu again.

## Drawing

| Component | Draws |
|---|---|
| `Drawable` | Required: the layer (`World` or `Interface`) and the order inside it (higher on top). |
| `Text` | A text, centred on the position. |
| `Sprite` | A texture or a frame of a sprite sheet, centred on the position. |
| `Box` | A rectangle, filled or outlined, centred on the position. |
| `Scale`, `Opacity` | Size factor and transparency, animated by tweens. |

The HUD is made of entities of the game scene in the `Interface` layer: it is drawn after the world.

## Animations

A `Tween` animates the `Scale` or the `Opacity` of an entity, then is removed, unless it goes back and forth:

```cpp
ecs.addComponent<Tween>(enemy, Tween{.target = TweenTarget::Opacity, .from = 1.0f, .to = 0.2f,
                                     .duration = 0.1f, .pingPong = true});   // blinks when hit
```

| Easing | Shape |
|---|---|
| `Linear` | Constant speed |
| `EaseOut` | Fast, then slowing down |
| `EaseOutBack` | Goes slightly past the end, then comes back: a bounce |

The focused widget of a menu bounces to `InterfaceStyle::focusScale` with `EaseOutBack`.

## Menus

`MenuBuilder` opens a scene and creates a background, a title and widgets stacked in a column:

```cpp
MenuBuilder pause(ecs, scenes, metrics, style, "Pause", {.overlay = true});
const SceneId scene = pause.scene();
pause.button("Resume", [&scenes, scene] { scenes.close(scene); });
pause.toggle("Fullscreen", [&] { return window.isFullscreen(); }, [&](bool on) { window.setFullscreen(on); });
```

It only creates entities: an icon or an animation can be added to any of them afterwards.
`apps/client/src/Menus.cpp` builds the pause and options menus and is the model for new menus.

| Widget | Shows | Reacts to |
|---|---|---|
| `label` | A text that cannot get the focus | — |
| `button` | `Play` | Confirm, click |
| `toggle` | `Fullscreen: On` | Confirm and click switch it; Right on, Left off |
| `slider` | `Volume: 80%` | Left, Right and the wheel change it by steps |
| `choice` | `Colour filter: None` | Left, Right, the wheel and a click go through the options |
| `textField` | `Name: Player` | Typing edits it while focused, with cursor, selection and clipboard; Confirm submits it |

Values are read and written through functions, not copied: a toggle for fullscreen always shows the real
setting, even when it is changed by a shortcut key.

### Navigation

| Action | Keyboard | Gamepad (first one) | Effect |
|---|---|---|---|
| `Up`, `Down` | Arrows | D-pad, left stick | Moves the focus, skipping labels, around at the ends |
| `Left`, `Right` | Arrows | D-pad, left stick | Changes the value of the focused widget |
| `Confirm` | ++enter++ | Bottom face button | Activates the focused widget |
| `Back` | ++escape++ | Right face button, Start | Goes back from the scene |
| `Click` | Left mouse button | — | Activates the widget under the pointer |

- Only keys that do not type text are used, so that letters, ++space++ and ++backspace++ edit text fields.
- Holding a direction repeats it; sticks have a larger dead zone (0.5) than in game.
- The mouse focuses a widget only when it moves, so that a still pointer does not fight the keyboard.
- A click activates the widget under the pointer, not the focused one.
- Characters typed while no text field has the focus are dropped.

## Testing

`tests/ecs_world.cpp` tests the ECS features: elapsed time, references, deferred actions, hierarchy and
scenes. `tests/engine.cpp` builds menus with `MenuBuilder` and drives them with the `FakeInput` and fake text
measures, without a window: focus, every widget, layout, mouse, scenes, and the bounce of the focused widget.
