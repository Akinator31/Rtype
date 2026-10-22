# Scenes and interface

Everything the client draws is an entity of the ECS: the world, the HUD and the menus
([ADR 0007](../decisions/0007-client-in-the-ecs.md)). This page describes the client library built on the
[ECS](ecs.md) (`libs/engine`): scenes, drawing, animations and menus.

## Where things are

The client library is organised like the ECS, with one file per system:

```
libs/engine/include/rtype/engine/
  Component/    the data attached to entities
    Drawing.hpp     Layer, Drawable, Text, BoundText, Sprite, Box, Scale, Opacity
    Animation.hpp   Easing, ease(), Tween, Spin, ModelAnimation, switchModelAnimation()
    Widget.hpp      Focusable, Focused, Hitbox, Button, Toggle, Slider, Choice, TextField
    Layout.hpp      Column, ColumnItem, AnchorPoint, Anchor
    Toast.hpp       ToastType, Toast
  System/       the systems, one per file
    InterfaceSystem.hpp  WidgetTextSystem.hpp  BoundTextSystem.hpp  TweenSystem.hpp  HitboxSystem.hpp
    ColumnLayoutSystem.hpp  AnchorSystem.hpp  ToastSystem.hpp  SpinSystem.hpp  ModelAnimationSystem.hpp
  Interface/    what the interface is built with
    MenuBuilder.hpp  InterfaceBuilder.hpp  Navigation.hpp  Style.hpp  Metrics.hpp
  Rendering/
    SceneRenderer.hpp  ColliderDebugRenderer.hpp
  Scene/
    Scenes.hpp      SceneId, SceneMember, SceneSettings, Scenes
  Engine.hpp    includes everything above
```

The sources in `libs/engine/src/` follow the same folders. Every header compiles on its own: include the
precise headers you need, or `rtype/engine/Engine.hpp` for everything.

## The systems and their order

```cpp
Engine engine;
Ecs &ecs = engine.getEcs();
Scenes &scenes = engine.getScenes();
ecs.registerSystem<ConsoleSystem>(engine, devices);
ecs.registerSystem<InterfaceSystem>(ecs, scenes, devices, metrics, style);
ecs.registerSystem<GameSessionSystem>(ecs, scenes, engine.getControls());
ecs.registerSystem<WidgetTextSystem>();
ecs.registerSystem<BoundTextSystem>();
ecs.registerSystem<TweenSystem>();
ecs.registerSystem<SpinSystem>();
ecs.registerSystem<FlightSystem>();
ecs.registerSystem<IntroSystem>(scenes, devices);
ecs.registerSystem<ModelAnimationSystem>();
ecs.registerSystem<HitboxSystem>(metrics, style);
ecs.registerSystem<ColumnLayoutSystem>(ecs);
ecs.registerSystem<AnchorSystem>(metrics);
ecs.registerSystem<ToastSystem>(ecs, metrics, style);
ecs.registerSystem<TransformSystem>(ecs);
SceneRenderer sceneRenderer(ecs, scenes, style);

// Each frame
engine.update(window.frameTime());
renderer.beginFrame();
sceneRenderer.draw(renderer, Layer::World);
sceneRenderer.draw(renderer, Layer::Interface);
renderer.endFrame();
```

| # | System | Role |
|---|---|---|
| 1 | `Scenes` | Records which scene receives input for the whole update. |
| 2 | `ConsoleSystem`, `InterfaceSystem` | Console commands, menu focus, and navigation; they read the input before the game. |
| 3 | `GameSessionSystem` | The game: the input of the player to its session, the snapshots to the entities of the game scene ([A game in the client](client-game.md)). |
| 4 | `WidgetTextSystem`, `BoundTextSystem`, `TweenSystem`, `SpinSystem` | Texts of the widgets and bound texts, tweens, spinning models. |
| 5 | `FlightSystem`, `IntroSystem` | Client systems of the main menu: the ships flying around the station, the intro. |
| 6 | `ModelAnimationSystem` | Animations of skinned models: the `SceneRenderer` poses each model before drawing it, fading from the previous animation after `switchModelAnimation()`, unless the model already has this pose (entities sharing a model and an animation at the same time are posed once). `ModelAnimation::speed` 0 holds the pose. |
| 7 | `HitboxSystem`, `ColumnLayoutSystem`, `AnchorSystem` | Sizes, then layout: columns, then anchors, which need the final sizes. |
| 8 | `ToastSystem` | Places, fades and destroys the toasts. |
| 9 | `TransformSystem` | Positions of the children from their parents, last. |

The `SceneRenderer` is not a system: drawing happens after the update, between `beginFrame()` and `endFrame()`,
and may happen several times per frame (once per eye in VR).

## ECS features

The interface relies on features of the ECS described in [ECS](ecs.md): the elapsed time given to systems,
[entity references](ecs.md#entity-references) (`EntityRef`), [deferred actions](ecs.md#deferred-actions) (a
button that opens a menu has its action deferred) and the [hierarchy](ecs.md#hierarchy) (`attach()`,
`isHidden()`): the widgets of a menu are attached to its column, and nothing hidden is drawn or focused.

## Scenes

A scene groups the entities of a phase of the game: home screen, game, pause menu, options. Every entity of a
scene has a `SceneMember`; when the scene closes, the `Engine` destroys them all at once, at the start of its
next update (until then they are neither visible nor updated). Entities without it are **persistent**:
a star-field shared by the menus and the game survives every scene change.

Scenes are part of the engine (`rtype::engine::Scenes`, `rtype/engine/Scene/Scenes.hpp`), not of the ECS: the
ECS only stores entities and components, and knows nothing about what a scene is. `Scenes` itself only keeps
the stack and never touches the ECS: destroying the entities of a closed scene is the `Engine`'s job.

```cpp
const SceneId game = engine.getScenes().open({.updatesWhenCovered = true});
const entity_t ship = ecs.createEntity();
ecs.addComponent<SceneMember>(ship, game);
```

Scenes form a stack:

| Setting | Effect | Example |
|---|---|---|
| `overlay` | The scenes below stay visible | Pause menu over the game |
| `hidesInterfaceBelow` | With `overlay`, only the world of the scenes below stays visible, not their interface | Submenu over the 3D background of the main menu |
| `updatesWhenCovered` | The scene keeps updating under others | Network game under the pause menu |
| `onBack` | Called on Back instead of closing the scene | Game: opens the pause menu |

Systems ask `Scenes` what to do with an entity or a scene: `receivesInput()` (only the scene at the top),
`updates()`, `isVisible()`, `isInterfaceVisible()`.

!!! warning "The input owner is set at the start of the update"

    The scene that receives the input is recorded when the update starts, by the `Scenes` system. When Escape
    closes the pause menu, the game does not receive the input until the next update: it does not see the
    same Escape press and does not open the menu again.

### Named scenes

The `Engine` registers `Scenes` first and opens scenes by name ([ADR 0009](../decisions/0009-named-scenes-in-the-engine.md)).
A scene is declared once, with a function that builds its entities each time it opens:

```cpp
engine.createScene("game", {.updatesWhenCovered = true}, [](Engine &engine, SceneId scene) {
    const entity_t ship = engine.getEcs().createEntity();
    engine.getEcs().addComponent<SceneMember>(ship, scene);
});
engine.subscribeToEvent("game:open", physics, Reaction::Enable);
engine.subscribeToEvent("game:close", physics, Reaction::Disable);

engine.switchScene("game"); // closes every open scene, then opens the game
engine.pushScene("pause");  // opens the pause menu over the game
engine.popScene();          // closes the scene at the top
```

These requests take effect at the start of the next `Engine::update`, then the events `"<name>:open"` and
`"<name>:close"` of the scenes opened and closed are triggered, and their reactions applied in the same update.

## Drawing

### Collider and grid debug overlay

The client can draw visible physics colliders over the world layer. Press **F3** to toggle the overlay, or
start the client with `RTYPE_DEBUG_COLLIDERS=1` to show it immediately. The overlay draws AABB outlines in
green, sphere outlines in green, triggers in yellow, and the spatial grid in pale blue. Grid lines use the
same cell size as `CollisionSystem` and game coordinates, so they stay aligned with the broad phase.

`ColliderDebugRenderer` lives in `libs/engine`, reads ECS `Position` and `Collider`, and skips hidden
entities or entities in hidden scenes. It draws after the world and before the interface. The physics library
still has no rendering dependency. The current game scene is empty; collider outlines appear as soon as
the game creates entities with `Position` and `Collider`.

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

`MenuBuilder` creates a background, a title and widgets stacked in a column, in a scene it opens:

```cpp
MenuBuilder pause(ecs, scenes, metrics, style, "Pause", {.overlay = true});
const SceneId scene = pause.scene();
pause.button("Resume", [&scenes, scene] { scenes.close(scene); });
pause.toggle("Fullscreen", [&] { return window.isFullscreen(); }, [&](bool on) { window.setFullscreen(on); });
```

Or in a scene already open, such as a named scene in its build function:

```cpp
engine.createScene("main_menu", {}, [&](Engine &engine, SceneId scene) {
    MenuBuilder menu(engine.getEcs(), scene, metrics, style, "R-Type");
    menu.button("Play", [&engine] { engine.switchScene("game"); });
    menu.button("Quit", [&window] { window.requestClose(); });
});
```

It only creates entities: an icon or an animation can be added to any of them afterwards.

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

### Elements placed anywhere

`InterfaceBuilder` puts elements anywhere on the screen, in a scene already open: a HUD, a version number, a
button in a corner. It draws no background. Each element gets an `Anchor`: its own point `point` is put on the
same point of the screen, then moved by the offset (x to the right, y downwards). The `AnchorSystem` computes
the position every frame from the size of the element: its hitbox at scale 1 for a widget, otherwise its
text, sprite or box. An anchored entity must not have a `Parent`.

```cpp
engine.createScene("game", {.updatesWhenCovered = true}, [&](Engine &engine, SceneId scene) {
    InterfaceBuilder hud(engine.getEcs(), scene, style);
    hud.boundText([&] { return "Score: " + std::to_string(score); }, {AnchorPoint::TopLeft, 20.0f, 20.0f});
    hud.text("v0.1", {AnchorPoint::BottomRight, -20.0f, -20.0f});
    hud.button("Pause", [&engine] { engine.pushScene("pause"); }, {AnchorPoint::TopRight, -20.0f, 20.0f});
});
```

| Element | Creates |
|---|---|
| `text` | A `Text` with the font, size and colour of the style |
| `boundText` | A `Text` rewritten every frame from a function by the `BoundTextSystem` (score, lives, ping) |
| `box` | A filled or outlined `Box` |
| `sprite` | A `Sprite`: a texture or a part of it |
| `button` | A focusable `Button`; the buttons of a builder get the focus in the order they are created |
| `backButton` | A small square `Button` with a leftwards arrow (←), as high as the other buttons, to go back |
| `header` | The top of a secondary screen: a `backButton`, then the title to its right on the same line |

A `BoundText` can also be added to any entity with a `Text`, outside of the builders.

### Toasts

A toast is a short message shown in a corner of the screen for a few seconds, over every scene and across
scene switches. Showing one takes only its text, and optionally its type, which sets the colour of its accent:

```cpp
engine.showToast("Settings saved");
engine.showToast("Connection lost", ToastType::Error); // Info, Success, Warning or Error
```

The toast entity holds only a `Toast` (its text and type). Everything about its look lives in one place,
`InterfaceStyle::toast` (`ToastStyle`): corner, margin, sizes, colours, accent of each type, durations, slide,
maximum number shown. The `SceneRenderer` draws it from that style (`drawToast`), and the `ToastSystem` places,
fades and destroys it. To change how every toast looks, change the style, or `drawToast` for its shape; the
calls stay the same.

Toasts are stacked from the corner, the newest nearest to it; the others glide to their new place. Beyond
`maxVisible`, the oldest fade out early. In the client, the console command `toast [info|success|warning|error]
<text>` shows one.

### Navigation

| Action | Keyboard | Gamepad (first one) | Effect |
|---|---|---|---|
| `Up`, `Down` | Arrows | D-pad, left stick | Moves the focus, skipping labels, around at the ends |
| `Left`, `Right` | Arrows | D-pad, left stick | Changes the value of the focused widget |
| `Confirm` | ++enter++ | Bottom face button | Activates the focused widget |
| `Back` | ++escape++ | Right face button, Start | Goes back from the scene, even one without widgets: Escape in the game opens its pause menu |
| `Click` | Left mouse button | — | Activates the widget under the pointer |

- Only keys that do not type text are used, so that letters, ++space++ and ++backspace++ edit text fields.
- Holding a direction repeats it; sticks have a larger dead zone (0.5) than in game.
- The mouse focuses a widget only when it moves, so that a still pointer does not fight the keyboard.
- A click activates the widget under the pointer, not the focused one.
- Characters typed while no text field has the focus are dropped.

## Testing

The ECS has its own tests (see [ECS](ecs.md#testing)). `tests/scenes.cpp` tests the scenes, and
`tests/engine.cpp` builds menus with `MenuBuilder` and drives them with the `FakeInput` and fake text
measures, without a window: focus, every widget, layout, mouse, scenes, and the bounce of the focused widget.
