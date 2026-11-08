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
    Animation.hpp   Easing, ease(), Tween, Spin, Scroll, ModelAnimation, switchModelAnimation(), addModelAnimation()
    Widget.hpp      Focusable, Focused, Hitbox, Button, Toggle, Slider, Choice, TextField, SettingRow, KeyRow, SectionLabel
    Layout.hpp      Column, ColumnItem, AnchorPoint, Anchor
    Toast.hpp       ToastType, Toast
    Sound.hpp       SoundEffect
  System/       the systems, one per file
    InterfaceSystem.hpp  WidgetTextSystem.hpp  BoundTextSystem.hpp  TweenSystem.hpp  HitboxSystem.hpp
    ColumnLayoutSystem.hpp  AnchorSystem.hpp  ToastSystem.hpp  SpinSystem.hpp  ModelAnimationSystem.hpp
    ScrollSystem.hpp  SoundSystem.hpp
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
ecs.registerSystem<SoundSystem>(audio);
ecs.registerSystem<WidgetTextSystem>();
ecs.registerSystem<BoundTextSystem>();
ecs.registerSystem<TweenSystem>();
ecs.registerSystem<SpinSystem>();
ecs.registerSystem<ScrollSystem>();
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
| 3 | `GameSessionSystem`, `SoundSystem` | The game: the input of the player to its session, the snapshots to the entities of the game scene ([A game in the client](client-game.md)). Then the sounds: the `SoundSystem` plays each `SoundEffect` given to an entity and removes it, in the frame the game gave it ([Audio](audio.md#in-the-engine)). |
| 4 | `WidgetTextSystem`, `BoundTextSystem`, `TweenSystem`, `SpinSystem`, `ScrollSystem` | Texts of the widgets and bound texts, tweens, spinning models, scrolling skies. |
| 5 | `FlightSystem`, `IntroSystem` | Client systems of the main menu: the ships flying around the station, the intro. |
| 6 | `ModelAnimationSystem` | Animations of skinned models: the `SceneRenderer` poses each model before drawing it, fading from the previous animation after `switchModelAnimation()`, unless the model already has this pose (entities sharing a model and an animation at the same time are posed once). `ModelAnimation::speed` 0 holds the pose; `blendedAnimation` and `blendAmount` mix a second animation in, in step; `addModelAnimation()` plays a third one once over them (`addedAnimation`, removed after `addedDuration`): what it changes from its first frame adds to their pose, bone by bone (a recoil over a flight). |
| 7 | `HitboxSystem`, `ColumnLayoutSystem`, `AnchorSystem` | Sizes, then layout: columns, then anchors, which need the final sizes. |
| 8 | `ToastSystem` | Places, fades and destroys the toasts. |
| 9 | `TransformSystem` | Positions of the children from their parents, last. |
| 10 | `LoadingSystem` | Client, at startup: the `loading` scene fades the logo in over black with a gauge under it while `AssetPreload` loads the pictures and models in the background, then fades the gauge out. `main.cpp` runs frames until it is finished, builds the other scenes from the cache, then switches to the main menu, which opens on black around the same logo and fades in. |

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

The client can draw visible physics colliders over the world layer. Press **F3** or type `hitboxes [on|off]` in
the console to toggle the overlay, or start the client with `RTYPE_DEBUG_COLLIDERS=1` to show it immediately. The overlay draws AABB outlines in
green, sphere outlines in green, triggers in yellow, and the spatial grid in pale blue. Grid lines use the
same cell size as `CollisionSystem` and game coordinates, so they stay aligned with the broad phase.

`ColliderDebugRenderer` lives in `libs/engine`, reads ECS `Position` and `Collider`, and skips hidden
entities or entities in hidden scenes. It draws after the world and before the interface. The physics library
still has no rendering dependency. The `GameSessionSystem` gives each entity of the game scene the `Collider`
its session finds (`GameSession::findCollider()`): offline, the real collider of the entity in the `Simulation`;
online, the box it is drawn as, as the snapshots do not carry the hitboxes.

### Frame rate counter

Type `fps [on|off]` in the console to show or hide the number of frames per second, in the top right corner
of the screen, over the interface (`drawFpsCounter()`, `apps/client/src/Commands/FpsCommand.cpp`). It is green,
then orange and red as the frame rate drops. It checks the limit of frames per second and the vertical
synchronisation chosen on the Settings page ([Settings](settings.md#the-settings-page)).

Both views are kept in `DebugViews` (`Commands/Commands.hpp`), read by the game loop. The commands that show
and hide something read their `on` / `off` argument with `readSwitch()` (`Commands/Switch.hpp`).

| Component | Draws |
|---|---|
| `Drawable` | Required: the layer (`World` or `Interface`) and the order inside it (higher on top). |
| `Text` | A text, centred on the position. |
| `Sprite` | A texture or a frame of a sprite sheet, centred on the position, turned by its `rotation`. |
| `Box` | A rectangle, filled or outlined, centred on the position. A filled box with a `bottom` colour fades from its colour at the top into it: a shadow along an edge of the screen. |
| `Scale`, `Opacity` | Size factor and transparency, animated by tweens. |

The HUD is made of entities of the game scene in the `Interface` layer: it is drawn after the world.

### The sky

A `Backdrop` covers the visible area with a picture, behind the 3D models; it needs no `Drawable` nor
`Position`. The sky is the scene of the first shown `Backdrop`: all its `Backdrop`s and `BackdropSprite`s are
drawn by `order`, the lowest at the back.

| Component | Draws |
|---|---|
| `Backdrop` | A picture scaled to cover the visible area, cut on the sides that overflow. With a `Scroll`, it slides to the left by `Scroll::offset` and repeats itself: its right side must join its left side. |
| `BackdropSprite` | A picture of `height` game units, centred `y` units under the top of the visible area. With a `Scroll`, it comes in from the right, goes out on the left, and comes back every `period` units of scrolling; `start` is the scrolling already done at the start (0: just out of the screen, on the right). |
| `Scroll` | `speed` in game units per second; the `ScrollSystem` adds `speed × elapsed` to `offset`. |

Layers going faster the nearer they are make a parallax: the game scene scrolls four layers and three objects
([A game in the client](client-game.md#the-sky)).

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
| `passwordField` | `Password: ******` | A `textField` whose text is secret (`TextInput::setSecret()`): drawn as one `*` per character (`fieldContent()`), never copied nor cut to the clipboard; pasting works |

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
| `box` | A filled or outlined `Box`; `Box::bottom` or `Box::right` fade it into a second colour, downwards or to the right |
| `sprite` | A `Sprite`: a texture or a part of it |
| `button` | A focusable `Button` of text alone (`InterfaceStyle::flare`), as wide as its spaced letters and its star: when lit, a line of light traces itself across the text and a star flares after it; when pressed, the star bursts, a pulse runs along the line, and the action runs `pressDelay` later. The buttons of a builder get the focus in the order they are created |
| `dangerButton` | A `button` in the red of the alerts (`Panel::danger`, `FlareStyle::danger`): its text, its line and its star. For an action that deletes |
| `textField`, `passwordField` | A field of a form (`TextFieldType::Line`): see [Forms](#forms) |
| `codeField` | A code of digits in cells (`TextFieldType::Code`): see [Forms](#forms) |
| `listRow` | A row of a list (`ListRow`): see [Lists and avatars](#lists-and-avatars) |
| `settingRow`, `linkRow` | A row of a page of settings (`SettingRow`): see [Pages of settings](#pages-of-settings) |
| `keyRow` | A row of the table of the controls (`KeyRow`): see [Pages of settings](#pages-of-settings) |
| `sectionLabel` | The name of a group of rows and its rule (`SectionLabel`), or the head of a column of a table |
| `note` | A small text in grey (`SettingStyle::note`): a hint, the keys of a page |
| `avatar` | The round picture of a player, or their initial (`Avatar`): see [Lists and avatars](#lists-and-avatars) |
| `backButton` | A `Button` with a "<", to go back (the font of the buttons has no "←") |
| `header` | The top of a secondary screen: a `backButton`, then the title to its right on the same line |

A `BoundText` can also be added to any entity with a `Text`, outside of the builders.

### Toasts

A toast is a short message shown in a corner of the screen for a few seconds, over every scene and across
scene switches. A toast is a line of light with no panel: its subject and its actor in bright capitals, its
message in grey between them, and an icon at the end. The type sets the colour and the icon: a circled "i"
for `Info`, a star for `Success`, a triangle for `Warning` and `Error`, in red.

```cpp
engine.showToast("Settings saved");                    // a message alone is shown as the subject
engine.showToast("Connection lost", ToastType::Error); // Info, Success, Warning, Error or Quest
engine.showToast("ORBITAL MODULE", "taken by", "KAZE", ToastType::Success); // subject, message, actor
```

Two other kinds of notifications are shown with their own calls, and each kind has its own stack, with its own
maximum (`maxVisible`, `maxAnnouncements`, `maxBanners`):

```cpp
engine.showAnnouncement("PLAYER 2 JOINED", "Room · 2 / 4 pilots");     // title, line of light, detail
engine.showBanner("BOSS APPROACHING", "GET THE BEAM READY", ToastType::Error); // across the whole screen
engine.showBanner("WAVE 03", "BYDOS INCOMING", ToastType::Info, 1.5f);        // shown 1.5 s, not the style's
```

An announcement is centred at the top of the screen: its icon and title, a line of light under them, and its
detail in grey. A banner is a band across the whole screen, between two lines of light that open from the
middle, with a veil of light, its title in large capitals between two sparks and its detail under it. Their
look is in `ToastStyle` too (`announcement...` and `banner...`).

Both play an entrance made of parts that each have their moment, and play it backwards when they leave
(`announcementIn` / `announcementOut`, `bannerIn` / `bannerOut` in the style). An announcement: its icon pops, its
title fades in while its letters tighten (`spacingIn`), its line of light draws itself from the middle with a
flash, then its detail rises. A banner: a line of light draws itself from the middle, the band opens around it as
the two lines move apart, the sparks fly out from the middle, the title tightens its letters, then the detail
rises. The `ToastSystem` writes the progress of the entrance as the `Opacity` of the toast, and the
`SceneRenderer` splits it into parts (`stage()`).

The toast entity holds only a `Toast` (its words and type, and the time a banner stays fully shown when it is
given one: `Toast::duration`, `ToastStyle::duration` otherwise). Everything about its look lives in one place,
`InterfaceStyle::toast` (`ToastStyle`): corner, margin, fonts, sizes, colours of each type, durations, slide,
maximum number shown. The `SceneRenderer` draws it from that style (`drawToast`), and the `ToastSystem` places,
fades and destroys it. To change how every toast looks, change the style, or `drawToast` for its shape; the
calls stay the same.

Toasts are stacked from the corner, the newest nearest to it; the others glide to their new place. Beyond
`maxVisible`, the oldest fade out early. In the client, the console command `toast [info|success|warning|error|quest]
<text>` shows one.

### Titles and sliders

`InterfaceBuilder::title()` (and `boundTitle()`, for a title that follows the language) makes a text that the
`SceneRenderer` draws in chrome with a light around it, from `InterfaceStyle::title` (`TitleStyle`): the colour
goes from the top of the line to its bottom through the stops of `chrome` (`Renderer::drawGradientText()`).
`MenuBuilder` titles and the headers of the submenus use it.

`InterfaceBuilder::slider()` makes a value between 0 and 1 drawn as a row of a page of settings
(`InterfaceStyle::slider`, `SliderStyle`): its caption on the left, its percentage on the column of the values
of the other rows, and under them a fine rail with its filled part in light and a bar for the handle. The row
lights up as a `SettingRow` while it is under the pointer or has the focus; the handle grows when it is
pointed at, and more while it is dragged: the `InterfaceSystem` writes the `SliderTrack` of the slider, and
moves the value to the pointer while the mouse button is held. Left, Right and the wheel step the value as for
every `Slider`. The row is `SliderStyle::width` wide, or `SliderTrack::width` (the last parameter of
`slider()`); `getRailLeft()` and `getRailWidth()` give its rail.

`InterfaceBuilder::toggle()` makes an on / off setting drawn as "OFF - dot - ON" (`InterfaceStyle::toggle`,
`SwitchStyle`): the dot slides to the side that is on, and the label of that side lights up. The switch lights
up only while the pointer is over what is drawn of it, the labels and the link with a small margin
(`SwitchStyle::hoverMargin`; `computeSwitchLayout()` places the parts, for the drawing and for the pointer alike); a click anywhere on the row, Confirm, Left and Right
set it, as for every `Toggle`.

`InterfaceBuilder::gauge()` makes a value between 0 and 1 drawn as a rail lit from its start (transparent) to
its value, with a spark at the end (`InterfaceStyle::gauge`, `GaugeStyle`; a full gauge shines more). A gauge
shows only: it gets no focus. An `Extent` gives its size to the `AnchorSystem`. `Gauge::width` gives it its
own length instead of the one of the style, and `Gauge::low` a value at or below which its light and spark turn
red (`GaugeStyle::lowLight`, `lowGlow`). Without a caption, its rail is on the position of the entity: the
health bars of the game are such gauges, in the World layer.

An `engine::Ring` draws a circle lit along a part of its length, centred on its position: a dark track, then a
light from the right of the circle anticlockwise over the top, up to `Ring::value`, with a glow and a spark at
its end. Its owner writes the value: the countdown of the death screen of the game runs out this way. Like a
gauge, it takes an `Extent` to be placed by an `Anchor`.

`Text::spacing` spaces the letters of a text, as in the widely spaced capitals of the design; the
`AnchorSystem` and the `HitboxSystem` measure the text with it.

The handle of a slider lights up only while the pointer is over it (or it is dragged); its row lights up
while the slider is under the pointer or has the focus.

The client shows all of them, with the toasts, in the *Interface* menu of the settings
(`MainMenu/UiShowcase.cpp`).

### Pages of settings

The Settings and Controls pages of the client are made of rows that share one look (`InterfaceStyle::setting`,
`SettingStyle`), drawn by `SceneRenderer::drawRowFrame()`: a fine rule under each row, and while the row is lit
(its `Panel`, as a button), a veil from its left, a mark on its left edge and a line of light that traces
itself along its bottom. The caption is on the left, in the font of the text (Exo 2); the value is on the
right, in the font of the values (Audiowide), and every value ends on the same column
(`SettingStyle::getValueInset()`), so that the values of a page line up whatever their length.

| Element | Row |
|---|---|
| `settingRow(caption, value, change, anchor)` | A choice (`SettingRow`): `value` is read every frame, `change(+1)` or `change(-1)` goes to the next or previous value on Right, Left, the wheel, Confirm and a click (forwards). Chevrons appear around the value while it is lit |
| `linkRow(caption, onPress, anchor)` | A row that opens something: a chevron on its right, pressed as a button |
| `slider(...)` | A volume: see [Titles and sliders](#titles-and-sliders) |
| `keyRow(action, row, onPress, anchor)` | A row of the table of the controls (`KeyRow`, `InterfaceStyle::keys`, `KeyStyle`): the action, its keys each in a frame like a key of a keyboard (`KeyCap`, in the state `Normal`, `New`, `Waiting` in dashes, or `Conflict` in red), and the buttons of the gamepad in pills, read only. `KeyRow::dimmed` dims it |
| `sectionLabel(text, anchor, width)` | The name of a group, in small capitals, with a rule up to `width` |
| `note(text, anchor)` | A small grey text: the hint of the focused row, the keys of the page |

The client lays them out in `MainMenu/SettingsPage.hpp`: a veil (a `Box` that fades to the right) darkens the
side of the screen where the page is, so that its text reads over the planet of the main menu; the groups are
in columns, and the hint of the focused row and the keys of the page are notes. The design is the series 15 of
the Figma of the interface.

### Forms

`InterfaceBuilder::textField()` and `passwordField()` make a field of a form (`TextFieldType::Line`,
`InterfaceStyle::field`, `FieldStyle`): its caption in small capitals, its value under it in the font of the
buttons, and a fine rule under the value, which becomes a line of light while the field has the focus
(`TextField::light`, moved by the `InterfaceSystem` in `fadeDuration`). The field keeps its size, and its
`Text` stays its caption: the `SceneRenderer` draws its content itself, a password as one `*` per character.

The owner of the scene writes what is wrong in `TextField::error`: the caption, the value and the rule turn
red, and the error is written under the rule, in the room the field always keeps for it. The error is cleared
as soon as the player changes the content.

```cpp
const ecs::entity_t password = form.passwordField("PASSWORD", {AnchorPoint::TopLeft, 84.0f, 380.0f}, 128);
// The master refused it
engine.getEcs().getComponent<TextField>(password).error = "Wrong username or password";
```

`InterfaceBuilder::codeField()` makes a code (`TextFieldType::Code`): a cell per digit, as many as the maximum
length of its input, digits only. The cell being typed is lit; a pasted code fills the cells; the action is
called with the code as soon as the last cell is typed, or when the player confirms the field.

++tab++ moves to the next field, as Down.

### Lists and avatars

`InterfaceBuilder::listRow()` makes a row of a list (`ListRow`, `InterfaceStyle::list`, `ListStyle`), as wide as
asked: an avatar on its left if it has one, its title in capitals with a line of detail under it, a text on its
right, and a rule under it. With an action, it is a button: its `Panel` lights it as one, the rule becoming a
line of light that traces itself up to a star at its right end, and it gets the focus in the order of the
other widgets. Without an action, it is disabled: drawn dim, and skipped by the focus.

`InterfaceBuilder::avatar()` makes the round picture of a player (`Avatar`, `InterfaceStyle::avatar`,
`AvatarStyle`): a dark disc, the picture on it, or the initial without a picture, and a ring: silver for an
account, bright with a light around it for an account signed in with Discord, dotted for a guest. The picture
is a texture already round: `ResourceManager::loadRoundPicture()` makes one from a PNG received from the
network (the middle square, scaled, transparent outside of its circle), once per key.

The *Interface* menu of the settings shows a field, a password with an error, a code, two rows and the three
avatars.

### Navigation

| Action | Keyboard | Gamepad (first one) | Effect |
|---|---|---|---|
| `Up`, `Down` | Arrows (and ++tab++ for Down) | D-pad, left stick | Moves the focus, skipping labels, around at the ends |
| `Left`, `Right` | Arrows | D-pad, left stick | Changes the value of the focused widget |
| `Confirm` | ++enter++ | Bottom face button | Activates the focused widget |
| `Back` | ++escape++ | Right face button, Start | Goes back from the scene, even one without widgets: Escape in the game opens its pause menu |
| `Click` | Left mouse button | — | Activates the widget under the pointer |

- Only keys that do not type text are used, so that letters, ++space++ and ++backspace++ edit text fields.
- Holding a direction repeats it; sticks have a larger dead zone (0.5) than in game.
- The mouse focuses a widget only when it moves, so that a still pointer does not fight the keyboard.
- A click activates the widget under the pointer, not the focused one.
- Characters typed while no text field has the focus are dropped.
- The buttons sound when they light up (under the pointer, or given the focus by the keyboard or a
  gamepad) and when they are pressed, if the style has the sounds (`ButtonStyle::hoverSound`, `clickSound`,
  optional `SoundEffect`): the `InterfaceSystem` gives the `SoundEffect` to the button, and the `SoundSystem`
  plays it in the frame, before the deferred action of the button. A menu that appears lights its first button
  silently, otherwise every menu opened with the keyboard would sound twice. The sliders and switches (with a
  `SliderTrack` or a `SwitchTrack`) make the same sounds: the hover sound when they light up, the click sound
  when a slider is pressed with the mouse and when a switch is set to another value.

  ```cpp
  const SoundEffect hover{audio.loadSound("sounds/fx/button/hover.wav"), {.volume = 0.4f}};
  const SoundEffect click{audio.loadSound("sounds/fx/button/click.wav"), {}};
  const InterfaceStyle style{.button = {.font = buttonFont, .hoverSound = hover, .clickSound = click}};
  ```

- A `Panel` (the mark of a button) has a `highlight`: 0 for its normal look, 1 for its lit one, faded by a
  `Highlight` tween. It is how far the line is traced (`traceDuration` to light up, `fadeDuration` to go out);
  it goes above 1 when the button is pressed (`FlareStyle::pressedHighlight`), then back to 1 in
  `pressDuration`: the burst. Its action waits `pressDelay`, so that the burst is seen before a menu covers
  it; pressing again meanwhile does nothing.

## Testing

The ECS has its own tests (see [ECS](ecs.md#testing)). `tests/scenes.cpp` tests the scenes, and
`tests/engine.cpp` builds menus with `MenuBuilder` and drives them with the `FakeInput` and fake text
measures, without a window: focus, every widget, layout, mouse, scenes, the bounce of the focused widget, and
the sounds of the buttons.
